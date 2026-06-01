---
name: tpm-debug
description: Triage and debugging guide for the tpmkit C++17 library — interpreting TSS2_RC error codes, draining the OpenSSL error stack, swtpm log inspection, sanitizer-driven debugging, common deadlock and session-leak patterns, and reproducing flaky TPM tests. Use when investigating a failing test, an unexpected error code, a hang, a crash, or unexplained simulator behavior. Do not use for writing new tests, designing new code, or routine commit/PR work.
---

# Debugging — tpmkit

This skill covers triage and root-cause work: a test fails, a hang, a crash, an unexpected error code, an integration that worked yesterday and does not today. The goal is to get to the cause as fast as possible without flailing across tools. The always-on rules — `error-handling.md`, `concurrency.md`, `security.md` — describe how the library *should* behave; this skill is what to do when it is not behaving.

## Triage order

When a test or scenario is failing, work through these in order. Each step rules out a class of cause before the next. Skipping ahead is what makes debugging take three days instead of one hour.

1. **Reproduce deterministically.** A bug that happens "sometimes" is a bug that is not yet understood. Run the failing case three times in a row. If it does not reproduce, jump to the *Flaky tests* section before going further.
2. **Read the actual error.** What category was returned (`input_error`, `security_failure`, `resource_error`, `backend_error`)? What did the adapter log at the boundary *before* translation? The category tells you which layer to look at; the boundary log tells you what the third-party library said.
3. **Run under sanitizers.** ASan and UBSan catch a category of bugs that print as anything from "the test fails" to "the test passes but the next one fails." If the issue does not reproduce under ASan + UBSan, suspect a memory-model bug and run TSan as well.
4. **Bisect.** If reproduction is reliable and the issue is recent, `git bisect` against the test command. The bisect-friendly commit history (`tpm-commit-pr`) is the reason this works in practice.
5. **Inspect the simulator state.** For TPM-related issues, the swtpm logs and the TPM transient/persistent handle state are the next layer.

## Diagnosis vs. reproduction

Two questions get conflated. They have different answers, different tools, and treating them as one wastes hours.

- **Diagnosis** — *"what happened, on which path, with what failure mode?"* The log answers this. The categorical schema from `tpm-write-logging` (`event`, `component`, `algorithm`, `key_kind`, `session_kind`, `attempt`, `reason`, `source`, `error_code`, `backend_error_description`) plus the fixed message text narrows a production failure to a specific code path in minutes.
- **Reproduction** — *"can I trigger the same failure on my dev box?"* The log does **not** answer this, by design. Inputs to crypto/TPM operations are typically secrets, and the never-log rules forbid them at every level (cross-reference `security.md` Logging, `logging.md` What never to log). Reproduction uses different tools.

The mental shift: **the log narrows the search; the test infrastructure reproduces the bug.** In practice the path/category signal from the log is enough to run or write a targeted test that hits the same code path, without ever seeing the original input. Developers coming from non-crypto code expect "debug log → replay inputs in dev" and have to unlearn it here — every commercial crypto library has the same constraint.

### Tools for reproduction

Pick the tool that matches the bug class:

| Bug class | Reproduction tool |
|---|---|
| Algorithm-specific edge case (RSA modulus quirk, ECDSA nonce boundary, padding edge) | **Property-based test** (`rapidcheck`). Describe the input shape; let the framework generate values that hit edge cases. Re-run with the failing seed. Cross-reference `tpm-write-tests` Property-based tests. |
| Parser crash on caller-supplied bytes | **Fuzz corpus** (`libFuzzer`). Pin the crashing input under `tests/fuzz/corpus/<harness>/`; reproduce with `./fuzz_target corpus/<crash-id>`. Never delete a crash. Cross-reference `tpm-write-tests` Fuzz tests. |
| TPM-side state corruption | **swtpm state snapshot** captured at failure time, replayed on the dev box. The simulator state is the input that the host code alone cannot reproduce. |
| Memory or concurrency bug | **Sanitizer build** (ASan/UBSan/TSan). The bug usually surfaces under sanitizers with a stack trace and the relevant addresses, eliminating the reproduction step entirely. Cross-reference *Sanitizer-driven debugging* below. |
| Real customer input that no test framework can synthesise | **Customer reproducer through a support channel**, with the data sanitized or replaced with synthetic equivalents. Production logs are never the conduit — the support channel is. The customer's data does not leave their environment. |
| Ephemeral local dev case | **Local debugger** (`gdb`, `lldb`). Non-persisted, dev-only, single-developer. The right tool for "what value did this variable hold *right now* on my machine." |

### When the tools cannot reach the bug

If a real bug cannot be reproduced through any of the channels above, the actionable fix is **not** "log more raw inputs in production." It is one of:

- **Extend the property test generators** to cover the missed input shape. The shape was not in the test corpus; that is a test-infrastructure gap, not a logging gap.
- **Add a fuzz harness** for the input format that produced the crash, with the crashing seed pinned in the corpus. Cross-reference `tpm-write-tests` Fuzz tests.
- **Add a categorical field to the schema** that captures the missing signal as a *category*, not a literal value (`cache_state`, `pool_occupancy`, `state_transition`). Cross-reference `tpm-write-logging` Adding a log call site and the schema's *Versioning* rules — schema additions go through the stability rules and are part of the public surface.

A bug class that genuinely cannot be reproduced by any of these tools is rare and is itself a design signal: the affected port or adapter probably needs decomposing into smaller, more testable units. Cross-reference *When to escalate to the architecture* below.

## Reading TSS2_RC codes

A `TSS2_RC` is a 32-bit value with a layered structure: high bits identify the layer that produced the error, low bits identify the specific error. The library translates these at the adapter boundary, but the *original* `TSS2_RC` is what diagnoses the cause.

The layer field tells you where to look:

| Layer | Hex pattern | Meaning |
|---|---|---|
| `TSS2_TPM_RC_LAYER` | `0x0000xxxx` | Error came from the TPM itself (rejected the command). |
| `TSS2_FEATURE_RC_LAYER` | `0x0006xxxx` | FAPI layer. |
| `TSS2_ESAPI_RC_LAYER` | `0x0007xxxx` | ESYS layer. |
| `TSS2_SYS_RC_LAYER` | `0x0008xxxx` | SAPI layer (rare; below ESYS). |
| `TSS2_MU_RC_LAYER` | `0x0009xxxx` | Marshalling/unmarshalling. |
| `TSS2_TCTI_RC_LAYER` | `0x000Axxxx` | Transport (e.g., `swtpm` connection). |

Common code patterns to recognise on sight:

- `0x000` `0` `0xxx` — TPM-format codes. `0x000_NNN` is a TPM error from the FMT0 group; `0x000`*x*`9NN` is a TPM error from the FMT1 group (parameter-, handle-, or session-related).
- `TPM2_RC_AUTH_FAIL` (FMT1) — authorization HMAC mismatch. Almost always a wrong password, a session key drift from concurrent use (cross-reference: `concurrency.md` TPM-specific concurrency), or a session no longer valid.
- `TPM2_RC_BAD_AUTH` — authorization not allowed for this entity. The handle's auth policy does not permit this operation.
- `TPM2_RC_NV_DEFINED` — attempting to define an NV index that is already defined. Persistent handle ownership confusion (cross-reference: `security.md` TPM-specific concerns).
- `TPM2_RC_OBJECT_HANDLES` / `TPM2_RC_SESSION_HANDLES` — handle table exhausted. Session leak is the usual cause; see *Session leaks* below.
- `TSS2_TCTI_RC_IO_ERROR` — transport failed. swtpm is not running, the socket/path is wrong, or it crashed.

For tpmkit backend-boundary logs, read `backend_error_description` first for the sanitized `Tss2_RC_Decode` output, then use `error_code` as the canonical value if the decoded text is unavailable or too coarse. Do not guess from partial matches.

## Draining the OpenSSL error stack

OpenSSL accumulates errors on a thread-local stack. A failed `EVP_*` call returns a generic failure; the *reason* is on the stack. Adapters must drain it before returning so the next call is not contaminated by stale errors.

When debugging, dump the full stack at the failure point:

```cpp
unsigned long e;
while ((e = ERR_get_error()) != 0) {
    char buf[256];
    ERR_error_string_n(e, buf, sizeof(buf));
    fprintf(stderr, "OpenSSL: %s\n", buf);
}
```

Common patterns to recognise:

- **`error:0909006C:PEM routines:get_name:no start line`** — input is not PEM (likely DER, or wrong file).
- **`error:1E08010C:DECODER routines::unsupported`** — OpenSSL 3.x decoder cannot find a handler. Often a provider issue (legacy provider not loaded, or the input is in a format that requires it).
- **`error:0608A09E:digital envelope routines:EVP_PKEY_verify:operation not supported for this keytype`** — algorithm/key mismatch (e.g., trying ECDSA verify with an RSA key).
- A long stack with the *first* error being the actual cause and the rest being upstream consequences. Read the stack top-down; the first line is what you want.

If your adapter does not drain on failure, you will see errors from a previous test attributed to the current one. That is the bug — fix the drain, then re-debug.

## swtpm and simulator issues

For integration tests against `swtpm`:

- **Process is running but tests fail to connect.** Check the socket path matches what the TCTI expects. `swtpm socket --tpm2 --server type=unixio,path=/tmp/swtpm-socket --ctrl type=unixio,path=/tmp/swtpm-ctrl ...` — both sockets need to exist and be readable by the test user.
- **State persistence between tests.** swtpm persists state to a directory by default. Tests that assume a fresh TPM must launch swtpm with `--flags not-need-init` and a clean state directory, or call `TPM2_Startup(CLEAR)` themselves. Fixture-level state leaks are the most common source of "passes alone, fails in the suite" — see `tpm-write-tests` (no `SetUpTestSuite` for mutable state).
- **Hangs in a test that opens a session.** swtpm-side handle exhaustion. A previous test leaked a session and the suite did not flush. Inspect the swtpm log for `out of session handles` or restart swtpm between suites.
- **Mismatched TPM versions.** Pin swtpm version in CI alongside the dependency pins (`security.md` Dependencies and CVE tracking). A behavior difference between simulator versions is real and not your code's fault — but only if the version is pinned.

## Sanitizer-driven debugging

The sanitizer matrix (ASan, UBSan, TSan) is required for release (`security.md` Build hardening). It is also the fastest way to debug a class of issues:

- **ASan reports a heap-use-after-free involving an OpenSSL handle.** A `unique_ptr` with a custom deleter is being copied (it should be move-only), or two adapters share a handle they shouldn't (cross-reference: `concurrency.md` OpenSSL threading).
- **UBSan reports an unsigned-integer overflow at the public API.** A size field was not validated before arithmetic. Cross-reference: `security.md` Input validation at the public API boundary.
- **TSan reports a data race on a non-atomic shared variable.** A cross-thread access path that the type's contract did not document. Either narrow the contract (single-thread ownership) or add the synchronization the type promises (`concurrency.md`).
- **TSan reports a lock-order inversion.** Two locks, two threads, opposite acquisition order. Fix by enforcing a single project-wide order; see `concurrency.md` Lock ordering.

If a sanitizer reports a bug that the unit tests pass, the bug is real — sanitizers detect undefined behavior that may happen to work today. Do not silence sanitizer findings; fix them.

Do not profile under sanitizers (`performance.md` Measurement discipline). Performance is not measurable when ASan adds 2–3× overhead. Profile a release build with `-O3 -g`.

## Hangs and deadlocks

A hang is information. Capture a stack trace before doing anything else.

- **`gdb -p <pid>` then `thread apply all bt`** — gives you every thread's stack. Look for two or more threads stuck on `pthread_mutex_lock` or `__lll_lock_wait` — that is your deadlock.
- **`pstack <pid>`** on Linux for a quick read-only view if `gdb` is heavyweight.
- **macOS:** `sample <pid> 5` produces a 5-second sampling profile that is excellent for finding stuck threads.

Common patterns:

- **Two threads sharing a TPM context without serialization.** TPM contexts are not thread-safe (`concurrency.md` TPM-specific concurrency). One thread is mid-command, the other is waiting on the connection. Confine the context or guard it with a mutex.
- **A `std::async` future destroyed without `.get()`.** The destructor blocks waiting for the task. If the task is itself blocked on something the main thread is supposed to do, you have a self-deadlock.
- **A `std::thread` that was joined inside a destructor that runs on the same thread.** Self-join is undefined; on most platforms, it hangs. Always check `joinable()` and `get_id() != std::this_thread::get_id()` before joining inside cleanup paths.

## Session and handle leaks

The TPM has a small number of transient handles (typically 3 sessions, ~6 transient objects). Leaks are the cause of most "works once, fails on retry" symptoms.

- Run the failing test under a fresh swtpm. If it now passes, the previous run leaked.
- Inspect the swtpm log for handle-allocation messages around the failure.
- Look for code paths that open a session or load a key but do not have RAII cleanup. Anything held by a raw `ESYS_TR` should be wrapped in a RAII class with `Esys_FlushContext` in the destructor (cross-reference: `tpm-write-code` RAII wrapper, `code-standards.md` Memory and resources).
- For persistent handles: a deliberate leak is fine, but document who owns the handle and when it is evicted (`security.md` TPM-specific concerns). An undocumented persistent-handle write is a bug.

## Reproducing flaky tests

A test that fails 1 in N runs is the worst kind of bug — easy to ignore, hard to fix. Treat it the same as a deterministic failure but with extra steps:

1. **Run the test in a tight loop until it fails:** `for i in $(seq 1 200); do ./test_binary --gtest_filter=ThatTest.* || break; done`. If it never fails, double the count.
2. **Run with `--gtest_shuffle --gtest_repeat=N`** to randomize ordering and catch test-order dependencies.
3. **Run under TSan.** Many "flaky" tests are races that race more often under load.
4. **Inspect for nondeterminism.** Wall-clock checks, unseeded RNGs, network calls, filesystem state, ordering assumptions on hash-map iteration. Cross-reference: `tpm-write-tests` Test discipline (deterministic, no shared mutable state).
5. **Eliminate the cause; do not retry.** Retrying a flaky test in CI hides the bug. The bug ships to production where it cannot be retried.

## Coredumps and crash debugging

Crypto code in core dumps is sensitive — the dump may contain key material, plaintexts, or session state. Treat a core dump from a process handling secrets as a controlled artifact (cross-reference: `security.md` Secret handling).

- For *test* binaries on a *developer host*, core dumps are fine. Inspect with `gdb <binary> <core>` and use `bt`, `info threads`, `frame N`, `print var`.
- For *integration* against real hardware, disable core dumps (`ulimit -c 0` or `RLIMIT_CORE = 0`) and rely on the in-process error path.
- Strip secrets before sharing a dump. If a colleague needs the dump for analysis, scrub it or share the stack trace and relevant locals only.

## When to escalate to the architecture

A pattern of bugs in the same area is information about the design, not just the code. If you find yourself debugging the same kind of issue more than twice, stop and ask:

- Is the port shape wrong? (Method that "almost fits" but not quite — see `tpm-add-port-or-adapter` Workflow A.1.)
- Is the contract suite under-specifying behavior? (Mock and real adapters drift apart — see `tpm-write-tests` Contract tests.)
- Is the error category losing information the caller needs? (`error-handling.md` — single-operation fix is to give that operation a more specific return type.)

A bug that comes back in a slightly different shape after every fix is a design bug; another fix at the same layer is throwing money at it.

## Error Handling

* **Cannot reproduce after three attempts.** Stop trying to reproduce the same way. Jump to *Reproducing flaky tests* and run the failing case in a tight loop (`--gtest_repeat`, `--gtest_shuffle`) under TSan. Do not retry-in-CI as a workaround — the bug ships to production.
* **No clear error category in the failure output.** Drain the OpenSSL error stack at the failure point (see *Draining the OpenSSL error stack*) and decode the layer bits of the `TSS2_RC` (see *Reading TSS2_RC codes*). The category usually falls out of the layer once the raw code is visible.
* **Bug only reproduces under a sanitizer.** That is a real bug, not a false positive — sanitizers detect undefined behavior the unit tests happen to miss. Fix it. Do not suppress with `__attribute__((no_sanitize))` without a documented justification per `tpm-build-config` Error Handling.
* **`gdb` thread dump shows two threads on `pthread_mutex_lock`.** Deadlock. Identify the two locks and the inverted acquisition order; enforce the project-wide order from `concurrency.md` Lock ordering. Do not "fix" by removing one of the locks unless the analysis shows the type's contract changed.
* **Test passes alone, fails in the suite.** Fixture-level state leak — almost always a `SetUpTestSuite` holding mutable state, or a leaked TPM session/handle. Switch to per-test setup and ensure RAII covers every TSS2 handle (cross-reference *Session and handle leaks*).
* **Same area produces the same kind of bug more than twice.** Escalate to architecture (see *When to escalate to the architecture*). Another fix at the same layer is throwing money at a design bug.
* **Log does not contain enough detail to diagnose.** Do not add raw-input logging — the never-log rules forbid it. Pick the right reproduction tool from the *Diagnosis vs. reproduction* table (property test, fuzz corpus, swtpm snapshot, sanitizer build, customer reproducer) or add a *categorical* field to the schema per `tpm-write-logging` Evolving the event schema.
* **Coredump from a process handling secrets.** Treat as a controlled artifact (`security.md` Secret handling). Strip secrets before sharing; on integration hosts disable core dumps entirely (`ulimit -c 0`) and rely on the in-process error path.

## Cross-references

- `error-handling.md` — error categories, adapter translation, what to log at the boundary.
- `security.md` — never-log rules, secret handling around dumps, sanitizer matrix.
- `concurrency.md` — lock ordering, TPM context ownership, OpenSSL threading.
- `tpm-write-tests` — deterministic test discipline, mock vs. integration distinction.
- `tpm-write-code` — RAII wrappers for TSS2 handles, port shape decisions.
- `tpm-build-config` — sanitizer build configuration, swtpm version pinning.
