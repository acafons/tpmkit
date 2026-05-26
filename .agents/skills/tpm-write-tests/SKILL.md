---
name: tpm-write-tests
description: Testing rules for the tpmkit C++17 library — frameworks (GoogleTest+GMock / rapidcheck / libFuzzer), test tiers (unit, integration, contract, property, fuzz, known-answer, stress, interoperability), swtpm and hardware-tagged TPM integration, security testing (memory-leak detection, secret zeroization, oracle uniformity, allowlist enforcement, integer-overflow guards, RNG sanity, mlock verification, session encryption), periodic offline audit (Valgrind Memcheck with suppression-file discipline, NIST SP 800-22 RNG suite, extended fuzz campaign, multi-hour stress runs), the secret-leak sweep, regression-test discipline, and coverage expectations. Use when writing or modifying tests, deciding what kind of test is appropriate, or setting up test infrastructure. Do not use for production code work, benchmark authoring (see performance.md), debugging existing tests without writing new ones, or non-C++ test work.
---

# Testing — tpmkit

The hexagonal architecture in `.claude/rules/architecture.md` only pays off if testing rules enforce the split. Domain code must be testable with no third-party libraries linked.

## Procedure

Apply this skill in order when writing or modifying a test:

1. **Classify the test tier.** Pick exactly one from the layout below:
   * **Unit** (`tests/unit/`) — public API behavior, pure domain behavior, public testing helpers, or adapter-internal logic isolated from live backends. Domain unit tests link no third-party libraries; adapter and adapter-shaped testing-helper unit tests may include/link backend SDK headers or constants but must not call a live backend, simulator, filesystem, or network.
   * **Integration** (`tests/integration/<backend>/`) — a specific adapter against the real backend (OpenSSL, FAPI, ESYS, swtpm). Hardware-only cases live here, gated by a `Hardware*` tag.
   * **Contract** (`tests/contract/`) — one parameterized suite per port, instantiated against every adapter (mock, real, fallback). Includes the secret-leak sweep.
   * **Property** (`tests/property/`) — round-trips and invariants over generated inputs (rapidcheck).
   * **Fuzz** (`tests/fuzz/`) — libFuzzer harness for a parser of caller-supplied bytes.
   * **Known-answer** (`tests/kat/`) — published vectors for every algorithm on the `security.md` allowlist. Required when adding a new algorithm.
   * **Stress** (`tests/stress/`) — many-thread, many-iteration, long-duration harnesses for handle/session exhaustion and concurrency. Runs nightly only.
   * **Interoperability** (`tests/interop/`) — cross-tool round-trips against `openssl`, `tpm2-tools`, and vendor utilities. Catches encoding-layer drift.
   If the test seems to fit two tiers, it belongs in the lower one (unit beats integration; integration beats contract). Benchmarks are not tests — they live in `bench/` per `performance.md`.
2. **Pick the framework block.** Unit/contract → GoogleTest + GMock. Integration/stress/interop → GoogleTest, no mocks. KAT → GoogleTest parameterized over the vector set. Property → rapidcheck. Fuzz → libFuzzer. Benchmarks → Google Benchmark (in `bench/`, not `tests/`). Do not mix frameworks within a tier.
3. **Apply the test discipline.** Deterministic, no shared mutable state, Arrange/Act/Assert, one behavior per test, `snake_case` names that describe behavior. `StrictMock<T>` is the default for ports — use `NiceMock<T>` only with a written rationale.
4. **Define `PrintTo` for new domain value objects** so GMock failure output
   is readable. Put the overload in the value object's namespace so ADL finds
   it, for example `namespace tpmkit::pcr` for `tpmkit::pcr::index`.
   Secret-bearing types must write `<redacted, N bytes>` only — never raw
   bytes. Cross-reference `security.md` and `tpm-write-code` value-object
   rules.
5. **Cover every flow.** Happy path, alternatives, and every error category from `error-handling.md`. Use `EXPECT_THROW`/`ASSERT_THROW` for contract-violation paths, not `EXPECT_DEATH`.
6. **Run under the sanitizer matrix** (ASan, UBSan, TSan) before opening the PR. A sanitizer failure is a release blocker per `security.md` Build hardening — fix the underlying bug, do not suppress.
7. **For new adapters or new ports**, run the contract suite against every adapter (including the mock) before merging. Adapters that pass the contract suite are interchangeable; adapters that don't are bugs waiting to happen.
8. **Keep the policy guard green.** Every test change must pass the `test_policy_guard` CTest entry. The guard fails missing top-of-test behavior comments, non-parameterized contract tests, `GTEST_SKIP()` in unit/contract/integration tests, deferred skip placeholders, and compile-only header smoke tests placed under `tests/integration/`.

## Frameworks and tools

- **Unit, integration, contract, stress, interop, and KAT tests:** GoogleTest + GMock. GoogleTest is the framework; GMock provides mocking for ports and collaborators (used in unit/contract only). Do not mix frameworks within the project.
- **Property-based tests:** rapidcheck.
- **Fuzzing:** libFuzzer for in-process fuzzing of parsers and deserializers.
- **Benchmarks:** Google Benchmark, in `bench/` (not `tests/`). Benchmarks have their own budgets, presets, and CI job per `performance.md` Measurement discipline; they are not part of the test suite and do not gate the test job.
- **Interop tooling:** invoked via subprocess from interop tests. Tool versions (`openssl`, `tpm2-tools`, vendor utilities) are pinned alongside swtpm in CI container images.
- **Coverage:** `gcov`/`llvm-cov` reported in CI. Failing the configured coverage threshold is a release blocker.

## Test layout

```
tests/unit/public_api/         installed API behavior and public-header smoke tests
tests/unit/domain/             domain tests, mock adapters only
tests/unit/testing/            backend-neutral public test-helper tests
tests/unit/testing/<adapter>/  adapter-shaped public test-helper tests; no live backend
tests/unit/<adapter>/          isolated adapter-internal unit tests; no live backend
tests/unit/logging/<backend>/  isolated logging adapter unit tests; no live backend
tests/integration/<adapter>/   real adapter tests (swtpm/hardware/backend as needed)
tests/contract/                shared suite run against every adapter for each port
tests/property/                property-based tests for domain primitives
tests/fuzz/                    libFuzzer harnesses
tests/kat/                     known-answer vectors per algorithm (NIST CAVP, RFC, TCG)
tests/stress/                  stress and soak tests; nightly CI only
tests/interop/                 cross-tool interoperability tests
bench/                         Google Benchmark suites (see performance.md)
```

## What gets tested where

- **Public API unit tests** — installed headers, value types, configuration
  objects, and public API contracts. Header smoke tests live here when they
  compile public headers directly. For component-family APIs, tests use the
  nested public namespace directly (`tpmkit::pcr::*`, `tpmkit::nv::*`,
  `tpmkit::key::*`) and should not rely on root compatibility aliases unless a
  compatibility plan explicitly added them.
- **Domain unit tests** — every domain function and use case, exercised through ports against mock adapters. No third-party library linked. Full suite runs in under a second.
- **Testing-helper unit tests** — public `tpmkit::testing::*` helpers. Backend-neutral helpers live under `tests/unit/testing/`; helpers shaped around an adapter or third-party ABI live under `tests/unit/testing/<adapter>/` (for example `tests/unit/testing/tpm2_esys/`).
- **Adapter-internal unit tests** — pure translation, validation, schema, and ABI-shim logic for a specific adapter. Place these under `tests/unit/<adapter>/` (for example `tests/unit/tpm2_esys/`); grouped adapter families mirror the source layout, as logging does under `tests/unit/logging/<backend>/`. They may compile against the backend SDK when needed for constants or function signatures, but they do not open real backend resources; anything that needs swtpm, hardware, or a real library call belongs in the integration tier.
- **Adapter integration tests** — verify each adapter satisfies its port using the real backend. Skipped only when the backend is unavailable on the platform.
- **Contract tests** — one shared suite per port, run against every adapter (mock, real, software fallback) to catch divergence between mock and real behavior. Implement with GoogleTest parameterized tests (`TEST_P` + `INSTANTIATE_TEST_SUITE_P`), one instantiation per adapter — never copy-paste the suite per backend. The **secret-leak sweep** lives here.
- **Property-based tests** — applied to round-trips (encode/decode, sign/verify, encrypt/decrypt). Generators must reach edge cases (empty, max-size, boundary values).
- **Fuzz tests** — every parser of caller-supplied bytes (key serialization, certificate parsing, policy parsing) has a fuzz harness that runs continuously in CI.
- **Known-answer tests (KATs)** — every cryptographic primitive on the `security.md` allowlist is exercised against its published vectors (NIST CAVP for AES-GCM/SHA-2/HKDF/ECDSA/RSA-PSS, RFC vectors for ChaCha20-Poly1305 and Ed25519, TCG vectors for TPM-side operations). Adding a new algorithm to the allowlist is gated on its KAT suite landing in the same PR — without vectors, intent is not enforcement.
- **Stress tests** — many-thread, many-iteration, long-duration harnesses targeting handle/session exhaustion, TSan-under-load, and resource exhaustion paths. Run in nightly CI only, not per-PR. Cross-reference `concurrency.md` Testing implications.
- **Interoperability tests** — verify signatures, ciphertext, and persisted keys round-trip with external tooling (`openssl pkeyutl`, `tpm2-tools`, hardware vendor utilities). Interop tests catch encoding-layer mistakes (DER mis-tagging, OID confusion, ASN.1 length errors) that pass internal contract tests because both adapters produce the same wrong output.

## Test discipline

Common to every tier (unit, integration, contract, property, fuzz):

- Tests are deterministic. No reliance on wall clock, network, or unseeded RNGs. Fix flakes; never retry.
- No dependencies between tests — every test must be runnable in isolation. No shared mutable state; each test owns its setup and teardown. Per-test `SetUp`/`TearDown` (or `TEST_F` fixtures) are fine; `SetUpTestSuite`/`TearDownTestSuite` are forbidden except for **read-only, immutable** setup (e.g., loading a known-good public key once). Anything mutable must be per-test.
- Structure each test as **Arrange / Act / Assert** (or **Given / When / Then**), with the three blocks visually distinct.
- Place a single short behavior comment as the first nonblank line inside every `TEST`, `TEST_F`, and `TEST_P` body. The comment describes the behavior under test, not the AAA section name. Example: `// Verifies invalid TCTI config is rejected before adapter calls.`
- One behavior per test. Avoid very large tests — split when a single test grows past the pattern. Multiple `EXPECT_*` lines are fine if they verify the same behavior.
- Tests assert behavior through the public API or the port being tested. Do not reach into private members or singletons.
- Test names describe behavior, not implementation: `signs_with_persistent_key`, not `calls_esys_sign_then_flush`.
- Both arguments to GoogleTest's `TEST(suite_name, behavior_name)` and `TEST_F(fixture_name, behavior_name)` use `snake_case`, matching `code-standards.md`. Suite name is the system under test; behavior name is the behavior. Example: `TEST(key_provider_fapi, signs_with_persistent_key)`.
- A single short comment at the top of each test states what is being tested. One sentence, not a docstring.
- Cover all flows: happy path, alternatives, and error/exception paths. Do not skip a flow because it is "obvious."
- Set consistent expectations: every claim the test makes is backed by an assertion, and no assertion checks incidental state unrelated to the claim.
- If a test feels too simple for the behavior it is supposed to cover, the test is wrong — rethink the design and write the test properly. Do not paper over complexity with a thin assertion.
- Secrets must never appear in test output. `secret_buffer` has no streaming operator (see `security.md`) and provides a `PrintTo(const secret_buffer&, std::ostream*)` overload that writes `<redacted, N bytes>`, so GMock failure messages cannot dump raw bytes. Any value object that contains or is derived from a secret follows the same rule.
- Domain value objects (digests, signatures, key handles, algorithm identifiers) define a `PrintTo` overload so GMock failure output is readable. Without one, GMock falls back to a byte dump — unreadable for digests and dangerous for anything secret-adjacent.
- The full test suite must pass clean under the project sanitizer matrix (ASan, UBSan, TSan). A sanitizer failure is a release blocker — see `security.md` (Build hardening).
- **Every bug fix lands with a regression test.** The test must fail before the fix and pass after — verify the fail-state on a separate commit on the branch, or via `git stash`/`git revert` locally before merging. Crashing inputs from fuzz harnesses are pinned in the corpus per the fuzz-tier rule below. TSan findings get a stress-tier reproducer. KAT failures get the offending vector pinned in `tests/kat/<algorithm>/regressions/`. Without a regression test the same bug returns under a different commit.

Unit tests (`tests/unit/`, `tests/contract/` against mock adapters):

- Use **GoogleTest + GMock**. Mock every collaborator that is not the system under test, including ports — that is what the mock adapters under `src/adapters/mock/` are for.
- Intercept calls in mocks to validate the **arguments** passed, not just the call count. Use `EXPECT_CALL(...).With(...)` matchers, or capture with `SaveArg`/`SaveArgPointee`, wherever the contract specifies what should be sent.
- All external dependencies are mocked. No network, no filesystem, no real OpenSSL/TSS calls — those belong in the integration tier. Adapter-internal and adapter-shaped testing-helper unit tests may include third-party headers or link SDK libraries only to compile constants, types, or ABI-compatible fake handles; they must not exercise a live backend.
- Mocking presupposes runtime polymorphism. GMock can only mock virtual methods (option 1 in `architecture.md`). Ports that need test-time substitution must use option 1; compile-time ports (option 2) and build-time-selected backends (option 3) are not GMock-compatible — they require hand-written stubs satisfying the concept, or a separate test build that links the test stub instead of the real adapter, respectively.
- Default to `StrictMock<T>` for ports — an unexpected call is a contract violation and the test should fail. Use `NiceMock<T>` only with a written rationale (e.g., a `logger` port whose calls are deliberately ignored). Plain `Mock<T>` (warnings on unexpected calls) is never the right default.
- Contract violations in this library throw (see `error-handling.md`); test them with `EXPECT_THROW`/`ASSERT_THROW`, not `EXPECT_DEATH`. Death tests apply only to debug-build `assert` failures and are not required for release behavior.

Integration tests (`tests/integration/`):

- **Do not mock.** The point is to exercise the real backend (OpenSSL, FAPI, ESYS, swtpm). If a test needs a mock, it belongs in the unit tier.
- Each test sets up and tears down its own real-environment state (TPM session, key, file). No fixtures that persist across tests.
- Cover the same flow set as the unit tier — happy path, alternatives, errors — but against the real backend.

Fuzz tests (`tests/fuzz/`):

- Each parser of caller-supplied bytes (key serialization, certificate parsing, policy parsing) has its own libFuzzer harness.
- The corpus lives at `tests/fuzz/corpus/<harness>/`. Seeds cover known-good inputs and edge cases. Crashing inputs found by CI or in the wild are pinned in the corpus as regression cases — never delete a crash.
- Harnesses build and run cleanly under ASan + UBSan. A crash that does not reproduce under sanitizers is still a bug.

Known-answer tests (`tests/kat/`):

- Vectors live in `tests/kat/<algorithm>/` (`aes_gcm/`, `ecdsa_p256/`, `rsa_pss/`, `chacha20_poly1305/`, `ed25519/`, `sha256/`, `hkdf/`, etc.). Each algorithm directory ships a `VECTORS.md` naming the upstream source (NIST CAVP file, RFC number, TCG spec reference, retrieval date).
- Vectors are stored **verbatim** in their published format. Do not regenerate; do not hand-edit. A wrong vector is filed upstream, not patched locally.
- Implemented as GoogleTest parameterized tests (`TEST_P` + `INSTANTIATE_TEST_SUITE_P`) instantiated over the vector set. A new vector file is loaded automatically; the test name encodes the upstream identifier so a failure points at the exact vector.
- **Adding a new algorithm to the `security.md` allowlist is gated on a KAT suite landing in the same PR.** A reviewer rejects an allowlist change without vectors — intent is not enforcement.
- A failing KAT is a release blocker. The vectors are authoritative; do not "fix the vectors to match the output."
- Regressions get the offending vector pinned under `tests/kat/<algorithm>/regressions/` with the bug reference in `VECTORS.md`.

Stress tests (`tests/stress/`):

- **Run nightly only**, not per-PR. The matrix runs each stress harness for a wall-clock budget (typically 10–30 minutes) under TSan.
- Target classes: (1) handle and session exhaustion (open-close cycles against swtpm until handle tables tighten), (2) concurrent adapter use (many threads sharing a TPM context guarded by the adapter's documented mutex, per `concurrency.md` TPM-specific concurrency), (3) OpenSSL handle reuse and per-thread allocation patterns (per `concurrency.md` OpenSSL threading), (4) long-running operations across thread hops with `request_id` propagation (per `tpm-write-logging`).
- A nightly failure is treated as a release blocker on the next release. Do not silence; do not retry. File a bug with the stress-tier reproducer attached.
- Stress harnesses cannot reach into private state; they exercise the public API and the ports the same way unit/integration tests do — what differs is the load profile.

Interoperability tests (`tests/interop/`):

- Cross-tool round-trips: signatures verified by `openssl pkeyutl -verify`, keys read by `tpm2-tools`, ciphertext decrypted by reference implementations, certificates parsed by `openssl x509`.
- Each test invokes the external tool via a subprocess with **pinned versions**. Tool versions are pinned in CI container images, alongside swtpm.
- Interop tests catch encoding-layer drift (DER mis-tagging, OID confusion, PEM header drift, ASN.1 length errors) that contract tests miss because both internal adapters produce the same wrong output.
- A test that requires a tool not present on the host is **skipped, not failed**. Gate on tool availability, not platform — tagged with the tool name (`Interop_openssl`, `Interop_tpm2_tools`) so CI can route per-runner.
- Do not parse the external tool's stdout as a structured channel — match exit code, then inspect stderr only for diagnostics. Tool output formats drift across versions.

## TPM-specific testing

- The mock `key_provider` adapter is the default for domain tests. It must implement the same contract as real adapters, validated by the contract suite.
- Real-TPM tests run against `swtpm` in CI. Pin the simulator version alongside the dependency pins.
- Tests that require a real hardware TPM are tagged with a `Hardware*` label and skipped by default; CI runs them only on hardware-equipped runners. Hardware classes are tagged further: `HardwareDTPM` (discrete TPM 2.0), `HardwareFTPM` (firmware TPM, e.g., Intel PTT / AMD fTPM), `HardwareVTPM` (cloud or KVM virtual TPM). A test that depends on a specific class names it explicitly; tests that work across all classes use the generic `Hardware` tag.
- Hardware-tagged tests still follow the per-test setup/teardown rule. Even on hardware, no fixture persists mutable state across tests — a leaked persistent handle from one test contaminates the next runner.

## Contract-tier specifics

The **secret-leak sweep** is a contract-tier test that drives every adapter through a controlled scenario exercising every event from `tpm-write-logging` `references/event-schema.md`, captures the records via the recording logger adapter, and scans field values for known-secret patterns — a hex sequence matching a generated test key, a known PIN, a deliberately tagged "canary" byte string injected before the call. Any match fails the suite. Lives under `tests/contract/secret_leak_sweep.cpp` and runs as part of every PR — it is the primary mechanical enforcement of the `security.md` never-log rules.

## Security testing discipline

Security testing is **cross-cutting, not a tier of its own.** Each kind of security test lives in the existing tier whose framework and run-cadence match it; this section lists each one so contributors know where to put a new security test, and reviewers know what to look for. Cross-reference `tpm-security-review` for the corresponding review-time checklist.

### Memory-leak detection

- Every test runs under ASan with `detect_leaks=1` (set in CI per `tpm-build-config` Sanitizers). A leak detected by ASan fails the test — there is no quarantine tier for "known leaks."
- Adapter and integration tests specifically exercise the RAII teardown path for every C-library resource: `EVP_*` handles, TSS2 contexts, `ESYS_TR` session handles. The test design scopes the resource into a block and asserts the post-block state, so the destructor is guaranteed to run.
- The stress tier runs under ASan with `leak_check_at_exit=1`. A leak that only grows under sustained load is caught by the stress tier, not the per-PR sanitizer job.
- Third-party leaks (OpenSSL, TSS2 internals) are not suppressed without an upstream bug reference and a documented justification per `security.md` Build hardening.

### Secret-zeroization tests

- Verify `secret_buffer` actually clears on destruction. The test populates a buffer with a known pattern, captures the address of its underlying storage, lets the buffer go out of scope, then reads the bytes at that address and asserts the pattern is gone.
- The clear must defeat dead-store elimination. Use `OPENSSL_cleanse` (anti-elision is documented and audited), never `std::memset` — the latter is legally elided by the optimizer once the buffer is no longer reachable.
- Every derived secret type (passphrase wrappers, HMAC keys, session keys) ships with its own zeroization test next to its definition.
- Lives in `tests/unit/secret_buffer/` and adjacent.

### Oracle-uniformity tests

- Every distinguishable cause of `security_failure` (wrong MAC, wrong signature, wrong padding, wrong policy hash, exhausted policy quota, expired session) must produce *indistinguishable* caller-visible output: same outcome category, same message text, same log-record shape, same coarse `source` value.
- Implemented as a parameterized test in `tests/contract/oracle_uniformity.cpp`, one parameter per failure cause, asserting the outcome and the recorded log fields are byte-for-byte equal across causes.
- Asymmetry is the bug. Do not relax the test — equalize the failing path's output. Cross-reference `error-handling.md` Security-sensitive failures and `tpm-write-logging` `references/event-schema.md` Forbidden combinations.

### Allowlist-enforcement tests

- Every algorithm identifier rejected by `security.md` (MD5, SHA-1, RC4, DES/3DES, AES-ECB, RSA-PKCS#1 v1.5 signing, raw ECB modes) is tested at the public API: requesting it returns `input_error` and never reaches an adapter.
- The negative-test set updates in the same PR as any allowlist change. A new forbidden algorithm without a negative test is an allowlist gap. Cross-reference `tpm-security-review` Section 5.
- Lives in `tests/unit/public_api/`.

### Integer-overflow and size-validation tests

- Public-API entry points taking a size or length are tested at boundary values: `0`, `1`, the documented maximum, the documented maximum + 1, `SIZE_MAX / 2`, `SIZE_MAX`.
- Out-of-range values reject with `input_error` **before any allocation**. The test asserts no allocation occurred (allocator-mock injected via the public API's allocator hook, or an RSS-watching smoke).
- Multiplication and addition of caller-supplied sizes are tested with operand pairs near wraparound — checked-arithmetic helpers must trip; raw `+`/`*` on caller-supplied sizes is a fail per `security.md` Input validation.
- Lives in `tests/unit/public_api/`.

### TPM authorization-failure tests

- Wrong password, wrong HMAC, DA-lockout-engaged, expired session, and revoked policy all return `security_failure` with no caller-visible distinguishing detail.
- Lives in `tests/integration/tpm2_fapi/` and `tests/integration/tpm2_esys/` against swtpm. The DA-lockout configuration is set in the fixture, not the test, so the test is reproducible without coupling to global swtpm state.

### Session-encryption verification

- Where `security.md` mandates TSS parameter encryption for sensitive parameters traveling host↔TPM, an integration test asserts the session-attribute encryption flag was set on the wire — not just on the host-side request struct.
- Implemented via swtpm command-trace inspection: swtpm logs each command's attribute bytes; the test parses the trace and asserts the encryption flag on the documented commands.
- Lives in `tests/integration/tpm2_esys/`.

### RNG sanity tests

- 1 MiB of output from each documented RNG source (host RNG, `RAND_bytes`, `Esys_GetRandom`) is sampled per test run and checked: byte histogram is not exactly uniform (would imply a counter), bit count is within `[0.49 N, 0.51 N]` (monobit), no exact 64-byte repeats inside the sample.
- These are **regression-catchers, not RNG quality tests.** A constant-output bug (RNG returning zeros, RNG returning a counter) trips them; a subtly biased RNG passes. A full statistical battery (NIST SP 800-22, dieharder) belongs in *Periodic offline audit* below, not CI.
- Lives in `tests/unit/rng/`.

### Memory-locking verification

- On platforms that support `mlock`/`VirtualLock`, a unit test injects a mock OS-call port into `secret_buffer` construction and asserts the lock call was made on the buffer's pages.
- A second test asserts the matching unlock call precedes the zeroization on destruction.
- Platforms without page-locking support skip via the `Mlock` tag, not via runtime branching inside the test. Cross-reference `security.md` Secret handling.
- Lives in `tests/unit/secret_buffer/`.

### Running security tests locally

CI is not the only place these run. Every security test described above is invokable locally with the project's CMake presets — useful when reproducing a CI failure, gating a commit before push, or iterating on a new security test. The presets and their bundled runtime options are defined in `tpm-build-config` Sanitizers; this section describes which preset to use for which class of security test.

**Sanitizer-based tests (leak detection, race detection, UB).** Configure once, run as many times as needed; the presets bake in `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1` and the matching TSan/UBSan options from `tpm-build-config`.

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

The same shape works for `ubsan` and `tsan`. ASan and TSan cannot link together — pick one preset per build.

**Filtering to security-marked tests.** Each test in the table above carries the `Security` CTest label, plus a sub-label (`Mlock`, `Zeroization`, `Oracle`, `Allowlist`, `RngSanity`, `SizeValidation`, `SessionEnc`, `TpmAuth`). Filter to a subset:

```bash
ctest --preset asan --label-regex Security --output-on-failure
ctest --preset asan --label-regex 'Zeroization|Oracle' --output-on-failure
ctest --preset asan --tests-regex secret_buffer --verbose
```

**The secret-zeroization test must run at `-O2` or higher** to be honest. The Debug preset's `-O0` hides the dead-store-elimination class of bug the test exists to catch. Run with the `asan-release` preset (`-O2` with ASan instrumentation) when iterating on a zeroization test that passes under `asan` but is suspected to drift under optimization:

```bash
cmake --preset asan-release
cmake --build --preset asan-release
ctest --preset asan-release --tests-regex zeroization --output-on-failure
```

**Integration security tests (TPM auth failures, session encryption).** Require swtpm running. Start it in another terminal — the socket paths are the same ones the test fixtures expect:

```bash
swtpm socket --tpm2 \
  --server type=unixio,path=/tmp/tpmkit-swtpm-socket \
  --ctrl type=unixio,path=/tmp/tpmkit-swtpm-ctrl \
  --tpmstate dir=/tmp/tpmkit-swtpm-state \
  --flags startup-clear
```

Then run the integration tier:

```bash
ctest --preset asan --label-regex 'Integration_tpm2_esys|TpmAuth|SessionEnc' --output-on-failure
```

The integration fixtures assert the local swtpm `--version` matches the pinned major.minor before running, and skip with a clear message on mismatch — local version drift does not produce confusing failures.

**Stress tier (nightly in CI, manual locally).** Stress harnesses are not part of `ctest --preset asan` by default — they take too long. Run them under the dedicated stress preset and budget:

```bash
cmake --preset tsan-stress
cmake --build --preset tsan-stress
TPMKIT_STRESS_DURATION=10m ctest --preset tsan-stress --label-regex Stress --output-on-failure
```

Use a shorter `TPMKIT_STRESS_DURATION` (e.g., `1m`) while iterating; restore the default before pushing. The CI nightly uses the documented per-harness budget.

**Interop tier (skip when tools absent).** Tests auto-skip when their tool isn't on `$PATH`. Confirm what is available:

```bash
openssl version    # required for Interop_openssl
tpm2 --version     # required for Interop_tpm2_tools
```

Run with:

```bash
ctest --preset asan --label-regex Interop --output-on-failure
```

A `[ SKIPPED ]` line is the expected outcome on a host without the matching tool — that is not a failure. To actually exercise an interop test, install the tool at the pinned version (see the CI container images for canonical versions).

**Pre-push gate (recommended).** The minimum local check before pushing a security-relevant change:

```bash
cmake --preset asan && cmake --build --preset asan && \
  ctest --preset asan --label-regex 'Security|Contract' --output-on-failure
```

This runs every `Security`-labelled test plus the full Contract tier (including the secret-leak sweep and the oracle-uniformity test) under ASan. It catches the failure modes most likely to bite in CI without paying for the full sanitizer matrix. Run UBSan and TSan separately when the change touches arithmetic on caller sizes (UBSan) or anything threaded (TSan).

### What is NOT tested at runtime

Some security properties are enforced statically or at build time; runtime tests for them are noise.

- **`secret_buffer` is move-only.** Enforced by `static_assert` in the type's header. A runtime test on `std::is_copy_constructible_v<secret_buffer>` is redundant — the code would fail to compile if violated.
- **Constant-time comparison uses `CRYPTO_memcmp`.** Enforced by code review (`tpm-security-review` Section 2) and by a clang-tidy custom matcher configured in `tpm-build-config`. Timing-based runtime tests are statistically unreliable on shared CI runners and add flakes without catching the bug they target.
- **No third-party headers in `src/domain/`.** Enforced by the domain-isolation grep gate in `tpm-build-config` Invariant checks.
- **No exported-symbol drift.** Enforced by the symbol-export diff gate in `tpm-build-config` Invariant checks.

If a security property feels test-shaped but is in this list, the static/CI enforcement is intentional — it catches the issue earlier and more reliably than a runtime test could.

## Periodic offline audit

Some tests cost too much to run per PR but matter enough to run before every release. They are not part of the CI matrix; they are scheduled work — release-time at minimum, plus a quarterly cadence between releases — and the results live in `audit/<YYYY-MM-DD>/` under the repo for the same audit-trail discipline as a security review.

Cross-reference `tpm-release` Step 3 (security sweep) — the release procedure pulls from this section.

### Valgrind Memcheck sweep

**Cadence:** every release tag, plus quarterly between releases.

**Purpose:** catch uninitialized-memory reads that ASan does not, and exercise the *release* binary rather than an instrumented build. Complementary to ASan, not a replacement — Valgrind runs ~10–50× slower so it cannot live in the per-PR matrix. Run on Linux only; Valgrind has no working macOS support and no Windows support.

**Procedure:**

```bash
cmake --preset release
cmake --build --preset release
valgrind --tool=memcheck \
  --leak-check=full --show-leak-kinds=all \
  --track-origins=yes --error-exitcode=1 \
  --suppressions=audit/valgrind/tpmkit.supp \
  --gen-suppressions=all \
  ./build/release/tests/<test-binary>
```

Run against the unit, integration, and contract test binaries plus each stress harness. Capture stdout and stderr to `audit/<date>/valgrind-<test>.log`. The `--gen-suppressions=all` output goes into a scratch file for review — never copied into `tpmkit.supp` without the discipline below.

**Suppression-file discipline.** `audit/valgrind/tpmkit.supp` is a security-relevant artifact and is reviewed like a dependency manifest:

- Every suppression names the upstream component (e.g., `openssl-3.2`, `tss2-fapi-4.0`, `glibc-2.38`), the version it was added against, a one-line reason the warning is benign, and the reviewer who approved it.
- A new suppression follows the same review path as adding a third-party dependency (`tpm-build-config` Dependency management). The reviewer's name is recorded in the entry.
- After every dependency bump, every suppression against that dependency is re-validated: run Valgrind unsuppressed and confirm the warning still appears in the new version's code paths. Stale suppressions are deleted in the same PR as the bump.
- **Entropy, RNG, and uninitialized-memory warnings in cryptographic code are never suppressed without an upstream vendor reference explaining why the read is intentional.** This rule is the lesson of CVE-2008-0166 — for two years Debian's OpenSSL had a crippled RNG because a maintainer suppressed Valgrind warnings on entropy mixing that looked spurious but were load-bearing. No vendor reference, no suppression; if in doubt, file with the upstream and wait.

**Reading the output.** A clean run produces no error records and a leak summary with zero "definitely lost" and zero "possibly lost". "Still reachable" is informational on a short-lived test binary; on a long-running stress harness it is a signal worth investigating.

### Full RNG statistical suite

**Cadence:** every release tag.

**Purpose:** the unit-tier RNG sanity test catches constant-output regressions in 1 MiB. The full statistical battery catches subtle bias — a working-but-degraded RNG that passes the unit test but produces detectably non-uniform output at scale.

**Procedure:** generate ≥1 GiB from each documented RNG source (`RAND_bytes`, `Esys_GetRandom`, host RNG) into `audit/<date>/rng/<source>.bin`. Run **NIST SP 800-22** and **dieharder** against each file.

**Reading the output.** Statistical tests *are* statistical — an isolated low p-value over many sub-tests is expected. The threshold is the documented multi-test pass criterion in the SP 800-22 reference; do not invent ad-hoc thresholds. A genuine suite-level failure is a release blocker — capture the failing sample and the seed before filing.

### Extended fuzz campaign

**Cadence:** every release tag.

**Purpose:** the per-PR fuzz job runs each harness for ~10 minutes. An overnight campaign explores deeper input space and surfaces inputs the short job cannot reach.

**Procedure:** run each harness in `tests/fuzz/` for ≥8 hours under ASan + UBSan. New crashes pin in `tests/fuzz/corpus/<harness>/regressions/` per the fuzz-tier rule. The release does not ship until each new crash is fixed or has a documented deferral with a security-impact review.

### Stress harness full-cadence run

**Cadence:** every release tag.

**Purpose:** the nightly stress job runs each harness for 10–30 minutes. The release run extends each to its documented multi-hour ceiling, catching slow-growth leaks, fragmentation patterns, and DA-lockout edge cases that only appear at scale.

**Procedure:** `TPMKIT_STRESS_DURATION=8h ctest --preset tsan-stress --label-regex Stress`. Capture per-harness wall-clock and resident-set growth to `audit/<date>/stress-<harness>.csv`. Unbounded growth in either dimension is a release blocker.

### Audit-result lifecycle

- Audit results live in the repo under `audit/<YYYY-MM-DD>/` as plain text and committed alongside the release tag commit (not as a separate branch or out-of-tree store).
- A failed audit is a release blocker. There is no quarantine tier — fix the cause or defer the release.
- Audit results from previous releases are kept indefinitely for trend analysis. Do not prune. A quarterly between-release audit lands in the same `audit/<date>/` shape, with a one-line README naming it as a between-release audit.
- The release-time security sweep (`tpm-release` Step 3) references the latest audit directory by date so reviewers can compare release to release.

## Coverage expectations

- Domain code: every line reachable through the public API must be covered.
- Adapter code: success and failure paths through the third-party library must be covered. Error translation logic is the most important path to test.
- Coverage gaps are either tested or marked with a documented exclusion comment. No silent gaps.

## Error Handling

* **Test passes alone but fails in the suite.** Fixture-level state leak. The cause is almost always `SetUpTestSuite` holding mutable state, or a leaked TPM session/handle from a prior test. Switch to per-test `SetUp`/`TearDown` and confirm every TSS2 handle is held by a RAII wrapper. Cross-reference `tpm-debug` Session and handle leaks.
* **Test flakes (fails 1 in N runs).** Do not retry in CI — that hides the bug. Run under TSan and with `--gtest_shuffle --gtest_repeat=N` to surface the cause (race, unseeded RNG, test-order dependency, network/wall-clock reliance). Cross-reference `tpm-debug` Reproducing flaky tests.
* **Real adapter passes its integration tests but fails the contract suite.** The contract suite under-specifies, or the mock and the real adapter disagree. Tighten the mock's behavior to match the real adapter, re-run, and only then proceed. Adapters that diverge silently from the contract are the bug the contract suite exists to catch.
* **Want to write `EXPECT_DEATH` for contract-violation behavior.** Use `EXPECT_THROW`/`ASSERT_THROW` instead. Contract violations throw in this project (`error-handling.md`); death tests apply only to debug-build `assert` failures and are not required for release behavior.
* **GMock failure output dumps raw bytes of a value object.** Add a `PrintTo(const T&, std::ostream*)` overload. For secret-bearing types, the overload writes `<redacted, N bytes>` — never raw bytes. Cross-reference `security.md` and `tpm-security-review` Secret handling.
* **Want to mock a compile-time-polymorphic port (option 2) or a build-time-selected adapter (option 3).** GMock cannot mock these mechanically. Either hand-roll a stub satisfying the same shape (option 2) or link a test-only stub in a separate build (option 3). If neither fits, the port belongs on option 1 — cross-reference `architecture.md` and `tpm-add-port-or-adapter` A.1.
* **Coverage gap shows up in the report.** Either add a test that reaches it or mark it with a documented exclusion comment naming the reason. Silent gaps are not acceptable; they grow until the next audit catches them.
* **`test_policy_guard` fails.** Fix the test shape rather than relaxing the guard. Missing top comments need a one-sentence behavior comment inside the test body; contract tests must be `TEST_P` plus `INSTANTIATE_TEST_SUITE_P`; deferred `GTEST_SKIP()` placeholders must become real tests or be removed; compile-only header smoke sources belong in `tests/unit/public_api/` or a dedicated compile-smoke target, not `tests/integration/`.
* **A swtpm integration test wants to skip when the simulator is not running.** Do not skip. The CTest integration target owns the simulator lifecycle through the `tpm_stack` fixture (`tpm_stack_start` / `tpm_stack_stop`). If startup fails, the integration run fails because the required backend is unavailable.
* **Fuzz harness crashes on an input the corpus does not contain.** Pin the crashing input under `tests/fuzz/corpus/<harness>/` as a regression case before fixing the bug. Never delete a crash — the corpus is the regression history.
* **KAT failure on a previously-passing algorithm.** Release blocker. Do not regenerate the vectors; do not "fix the vectors to match the output." The published vectors are authoritative — the output is the bug. Bisect to find the regression and add the failing vector under `tests/kat/<algorithm>/regressions/`.
* **New algorithm added to the `security.md` allowlist without a KAT suite.** Reject the PR. Vectors must land in the same PR as the allowlist entry, with `VECTORS.md` naming the upstream source.
* **Stress test fails nightly but passes per-PR.** Expected when the bug surfaces only under sustained load — that is the entire point of the stress tier. File a bug and treat as a release blocker on the next release. Do not retry the run. Cross-reference `tpm-debug` Reproducing flaky tests.
* **Interop test fails after a third-party tool bump.** Run the previous-version tool side-by-side. If both versions fail, our output regressed — fix on our side. If only the new version fails, file upstream and pin the previous tool version until the upstream fix lands.
* **Interop test requires a tool not installed on the runner.** Skip with the matching `Interop_<tool>` tag, do not fail. Route the test to a runner with the tool via CI tagging, not by relaxing the assertion.
* **Hardware-tagged test passes on `HardwareFTPM` but fails on `HardwareDTPM`** (or vice versa). That is a backend-behavior difference, not a test bug. Either narrow the test's tag to the supported class with a code comment explaining the limit, or fix the adapter to handle both classes — never silently widen the tag set.
* **Bug fix lands without a regression test.** Reject the PR. The test must fail before the fix and pass after; without it, the same bug returns under a different commit. Cross-reference *Test discipline* — regression tests are not optional.
* **ASan leak under a previously-clean test.** Bisect to find the regression. Do not suppress. ASan suppressions require an upstream-bug reference and a documented justification per `security.md` Build hardening; the bar is high precisely because every accepted suppression masks future regressions in the same area.
* **Secret-zeroization test passes at `-O0` but fails at `-O2`.** The optimizer is eliding the destructor's clear. Switch from `std::memset` to `OPENSSL_cleanse` (the latter has documented anti-elision behavior). Cross-reference `security.md` Secret handling.
* **Oracle-uniformity test fails on one of N failure causes.** That cause leaks an oracle. Equalize the outcome shape (same message text, same field keys and values, same coarse `source`) at the failing code path. Do not "relax the test" — the asymmetry *is* the bug the test exists to catch.
* **Allowlist-enforcement test passes but the forbidden algorithm is still reachable via a non-public path.** Audit how the path is exposed. If internal callers can construct the forbidden identifier and bypass the public API, the boundary is wrong — either close the path or wrap it in a port whose adapter validates. Trust-internal-callers (`code-standards.md`) only applies once the boundary is correctly placed.
* **Integer-overflow test reaches the allocator before rejecting.** The validation order is wrong. Move the size check ahead of the allocation; the test exists specifically to catch the "allocate-then-check" inversion that lets a hostile size succeed before failing.
* **mlock test fails on a platform that lists itself as supported.** Either page-locking regressed on that platform (file with the OS) or the platform was wrongly listed. Update the support matrix in the README per `tpm-write-docs` Project-level documentation; do not silently skip the test.
* **Session-encryption verification fails after a TSS bump.** TSS may have changed the default attribute composition. Re-read the TSS release notes, update the test's expected attribute mask, and confirm the call site still requests encryption explicitly — a default change is not a license to drop the explicit request.
* **RNG sanity test flakes once in a long run.** It is not flake. 1 MiB is large enough that monobit and 64-byte-repeat checks pass with overwhelming probability on a working RNG; a single failure is a real signal. Capture the failing sample for forensics before re-running.
* **Local ASan reports a leak that CI does not, or vice versa.** Tool-version mismatch. Align the local compiler/sanitizer version with the CI container image's pinned version (per `tpm-build-config` Reproducible builds) before assuming the bug is in the code. If the versions match, the bug is real on the platform that reported it.
* **ASan `detect_leaks=1` is unsupported on the local host (older macOS / Xcode).** Known platform gap. Run leak detection on the Linux ASan job (locally via container or via CI) and rely on the macOS ASan build for use-after-free / out-of-bounds coverage only. Do not weaken the assertion in the test.
* **swtpm refuses to start with `socket already in use`.** Previous instance left socket files behind. Clean `/tmp/tpmkit-swtpm-socket`, `/tmp/tpmkit-swtpm-ctrl`, and `/tmp/tpmkit-swtpm-state/`, then relaunch. Do not work around by changing the socket path — the fixtures expect the documented paths.
* **TSan stress harness consumes the local machine.** Reduce `TPMKIT_STRESS_DURATION` and lower the harness's thread count via its `--threads` arg (or the matching env var). Stress harnesses are designed to saturate; debugging one locally usually wants a focused configuration, not the nightly budget.
* **Secret-zeroization test passes under `asan` but is being run at `-O1`.** Switch to `asan-release` (`-O2`). The bug class the test exists to catch (DSE on a clear that the optimizer believes is unobservable) requires optimization to surface.
* **Interop test fails locally because the installed tool is newer than the CI-pinned version.** Pin locally to the CI version using a containerised invocation, or accept the divergence and rely on CI as the source of truth for interop. Do not "update the test to match the new tool" — that decouples the test from the version the project supports.
* **Pre-push gate passes but CI fails on UBSan.** The pre-push gate (`Security|Contract` under ASan) does not cover UB on arithmetic over caller-supplied sizes. When changing public-API size/length handling, also run `ctest --preset ubsan --label-regex 'Security|SizeValidation' --output-on-failure` locally before pushing.
* **Valgrind reports a warning ASan did not.** Expected — that is why both run. The warning is real until proven otherwise: do not suppress before reading the stack trace, identifying the source line, and either fixing the code or filing the suppression with a vendor reference per *Suppression-file discipline*. Uninstrumented dependencies are the most common true-positive source.
* **Valgrind reports an entropy / RNG / uninitialized-memory warning in cryptographic code.** Treat as a critical signal until disproven. **Do not suppress without an upstream vendor reference** — CVE-2008-0166 is the cautionary tale. If the warning is in OpenSSL or TSS2 code, search the upstream issue tracker first; if nothing matches, file upstream and *wait* before adding a suppression. A "this looks fine" judgment is exactly the failure mode the rule blocks.
* **Suppression file conflicts after a dependency bump.** Re-validate every suppression against that dependency at the new version: run Valgrind unsuppressed, observe whether the warning still fires from the documented call path. Delete suppressions whose warnings are gone; refresh the reviewer line on suppressions whose warnings persist. A bump that leaves the suppression file unchanged is a smell — somebody skipped the re-validation.
* **Full RNG statistical suite reports a failure at release time.** Release blocker. Capture the failing sample, the seed, and the test parameters into `audit/<date>/rng/` before doing anything else. Do not re-run hoping the failure goes away — that is the failure mode of "we'll retry the flaky one in CI" applied to statistical evidence.
* **Extended fuzz campaign finds a new crash on release day.** Pin the input under `tests/fuzz/corpus/<harness>/regressions/` and treat the release as blocked. A documented deferral requires a security-impact statement (`tpm-commit-pr` PR description template) — "trivial crash" without that statement is a release blocker, not a defer.
* **Stress harness shows unbounded RSS growth at the 8-hour budget.** Release blocker. The bug class (slow leak, fragmentation, or handle accumulation) only surfaces at this scale; deferring it ships the leak. Cross-reference `tpm-debug` Session and handle leaks.
* **Valgrind on macOS / Apple Silicon does not work.** Known platform gap. Run the Valgrind audit on a Linux host (CI container or release-build runner). Do not weaken the audit because of local-host limitations — that just moves the audit gap into the release.
