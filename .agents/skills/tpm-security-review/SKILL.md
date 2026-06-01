---
name: tpm-security-review
description: Project-specific security review checklist for the tpmkit C++17 library — auditing constant-time comparisons, secret_buffer usage, algorithm allowlist enforcement, public-API input validation, integer-overflow guards, error-message oracle leaks, RNG sources, TPM authorization handling, and all sanitizer/security-check findings against the rules in security.md. Use when reviewing a security-sensitive PR, performing a pre-release audit, validating changes under src/adapters/tpm2_* or src/adapters/openssl, or deciding whether memory leaks, buffer overflows, undefined behavior, data races, or failed security checks can pass. Do not use for general code review, performance review, or non-security architectural review.
---

# Security Review — tpmkit

This skill is the project's security-review checklist. The authoritative rules live in `security.md` and `error-handling.md`; this skill walks the reviewer through them in order so nothing is missed and so the review is reproducible across PRs.

This is a *project-specific* review, distinct from the generic `/security-review` slash command. The generic one finds OWASP-style issues a language-aware reviewer would catch; this one enforces the rules unique to a TPM and crypto library.

## When to run this review

- Any PR labeled `security:` or with `security impact != none` in the PR description (`tpm-commit-pr`).
- Any PR that touches: `secret_buffer`, public-API input parsing, signature/MAC verification, RNG calls, TPM session setup, persistent handle management, error messages on the `outcome<T>` boundary, anything under `src/adapters/openssl/` or `src/adapters/tpm2_*/`.
- Pre-release audits (cross-reference: `tpm-release` step on security sweep).
- Reviewing a change that adds or bumps a third-party dependency.

If a PR does not match any of the above, run the regular code review (`tpm-write-code`, `tpm-commit-pr`) and skip this skill.

## How to run the review

Walk the sections below in order. For each item, mark it as **pass**, **fail with location**, or **N/A with reason**. Do not skip an item because it "obviously passes" — the review's value is the audit trail, not the conclusion. Use the PR description's *Security impact* paragraph as a starting hypothesis, but verify it against the diff line by line.

The review's output is a comment on the PR listing every fail with a file:line reference and a short justification, plus the count of N/A items with their reasons. If everything passes, the comment still lists the sections checked — silent reviews offer no signal that the gate ran.

Hard gate: any memory-safety, sanitizer, fuzz, Valgrind, static-analysis, or security-check finding is a **fail** until the root cause is fixed and the relevant check is rerun cleanly. This includes memory leaks, buffer overflows/out-of-bounds access, use-after-free, double-free, uninitialized reads, undefined behavior, integer overflow, data races, secret leaks, oracle leaks, failed zeroization, failed allowlist checks, failed RNG checks, and failed session-encryption checks. Do not mark these as pass because they are "pre-existing," "external," "known," "flaky," or "unrelated" to the diff. Do not skip, quarantine, suppress, downgrade, retry-away, or waive them as part of this review.

## 1. Secret handling

Cross-reference: `security.md` Secret handling.

- [ ] Every variable that holds key material, a passphrase, a PIN, an HMAC key, or a TPM authorization value is typed `secret_buffer` (or `secret_string`). Grep the diff for `std::vector<uint8_t>`, `std::string`, `std::array<uint8_t,` and verify each instance is *not* secret-bearing.
- [ ] No `secret_buffer` is copied. The type is move-only; verify no diff resurrects copy operations.
- [ ] No `secret_buffer` reaches `std::cout`, `std::cerr`, a logger field, an exception message, a `to_string()`, or a test-output stream. Grep for `<<` adjacent to any `secret_*` identifier.
- [ ] If new types contain or derive from a secret, they have no streaming operator, and a `PrintTo(...)` overload that writes a redaction (cross-reference: `tpm-write-tests`).
- [ ] Destructors clear via `OPENSSL_cleanse` (or equivalent that the compiler cannot elide). A `std::memset` is not equivalent.
- [ ] `mlock` / `VirtualLock` is used on platforms that support it (or a documented justification for skipping is present).

## 2. Constant-time operations

Cross-reference: `security.md` Constant-time operations.

- [ ] Every comparison where one side is a secret, a MAC tag, a signature byte string, a password, or a capability token uses `CRYPTO_memcmp`. Grep the diff for `memcmp`, `==` on byte arrays, and `std::equal` and verify each is *not* secret-comparing.
- [ ] No secret-dependent branch in code we wrote (`if (key[i] == ...)`-style loops).
- [ ] No secret-indexed table lookup. Cross-reference: avoid building one in domain code; primitives are delegated to OpenSSL/TSS.
- [ ] On hot paths, no comparison was changed from `CRYPTO_memcmp` to `memcmp` "for performance." This is a security regression — flag it as a fail (cross-reference: `performance.md` What is not allowed).

## 3. Randomness

Cross-reference: `security.md` Randomness.

- [ ] No custom RNG. Grep for `rand(`, `srand(`, `mt19937`, `random_device`, `chrono::system_clock` adjacent to randomness use; each must be either absent or in a *non-cryptographic* context with an explicit comment.
- [ ] Cryptographic randomness uses `RAND_bytes` (OpenSSL) or OS-provided (`getrandom`, `BCryptGenRandom`).
- [ ] TPM-bound randomness uses `Esys_GetRandom` or FAPI equivalent — not host RNG.
- [ ] Every nonce-generating call site has a comment stating the uniqueness requirement and how it is achieved.
- [ ] No RNG seeded from PID, time, hostname, or anything else predictable.

## 4. Input validation at the public API

Cross-reference: `security.md` Input validation at the public API boundary.

- [ ] Every parameter at every public function in the diff is validated. Length checks, range checks, algorithm-identifier allowlists, OID allowlists.
- [ ] Unknown algorithm identifiers, OIDs, and curve names are rejected. No silent fall-through to a default.
- [ ] Size and length fields are checked *before* any allocation derived from them.
- [ ] Integer overflow is guarded for every multiplication or addition of sizes. `a + b` and `a * b` where either operand is caller-supplied are flagged unless wrapped in a checked helper. Verify `std::numeric_limits<T>::max()` is used appropriately.
- [ ] Stack buffers are explicitly initialized (`std::array<uint8_t, N> buf{};` — note the braces).
- [ ] Buffer-derived parameters use `gsl::span` (or the project's `byte_span`) or `.at()` access — never raw `[]` on indices that come from input.

## 5. Cryptographic algorithm choices

Cross-reference: `security.md` Cryptographic algorithm choices.

- [ ] No newly-added use of MD5, SHA-1, RC4, DES/3DES, AES-ECB, RSA-PKCS#1 v1.5 signing, or raw ECB modes for security purposes. SHA-1 may appear only for legacy TPM PCR bank compatibility, gated by `TPMKIT_ENABLE_LEGACY_SHA1_PCR`, and default builds must reject or filter it before exposing it as supported output.
- [ ] New algorithms not on the default-allowed list have an attached justification (PR description, ADR, or inline comment referencing `security.md`).
- [ ] Algorithm selection is *explicit* at the public API. No silent default that could change between versions.
- [ ] AES uses GCM (or ChaCha20-Poly1305); RSA uses PSS with ≥3072-bit keys; ECDSA uses P-256/P-384 or Ed25519.

## 6. Failure handling and oracle prevention

Cross-reference: `security.md` Failure handling, `error-handling.md` Security-sensitive failures.

- [ ] Security-relevant checks (signature verify, MAC validate, policy evaluate) **fail closed**. The diff does not contain a "best effort" path that proceeds despite a failed check.
- [ ] Public-API errors for security-sensitive failures return a single `security_failure` outcome with **no caller-visible detail** distinguishing the cause (no "wrong padding" vs. "wrong MAC" vs. "tag mismatch"). Detailed reasons are logged internally only.
- [ ] No error from OpenSSL or TSS is silently suppressed. Adapter code drains the OpenSSL error stack (cross-reference: `tpm-debug` Draining the OpenSSL error stack) and logs the original code plus sanitized decoded backend diagnostic text when available before translating.
- [ ] No exception thrown from inside a `secret_buffer` lifetime that could leave secrets uncleared. RAII covers this; verify no diff introduces a path that bypasses RAII.

## 7. Logging and side channels

Cross-reference: `security.md` Logging, `logging.md`.

- [ ] No log line at any level emits key material, plaintext, ciphertext, signature bytes, MAC tags, authorization values, PINs, passphrases, derived secrets, or pointer values that leak ASLR.
- [ ] Adapter-boundary logs include the original third-party error code, sanitized decoded backend diagnostic text when available, and library context, but not secret-derived bytes (e.g., no "expected MAC X, got Y").
- [ ] No log call inside a hot path or constant-time loop. Cross-reference: `performance.md` Hot-path discipline, `logging.md` Hot-path discipline.
- [ ] No new field key in a structured log record reveals a secret-derived value.
- [ ] If a new log field could leak in some configuration but not others, the configuration is documented and the default is the safe one.

## 8. Memory safety

Cross-reference: `security.md` Memory safety.

- [ ] No raw pointer arithmetic on caller-supplied buffers. Caller buffers are wrapped in `gsl::span`/`byte_span` at the API boundary.
- [ ] No `reinterpret_cast` from external bytes to a structured type without prior length and alignment validation. Prefer explicit deserialization.
- [ ] Indices derived from external input use `.at()`. `[]` only when the index is provably in range from local code.
- [ ] No `std::memcpy` to or from a `secret_buffer` raw pointer that bypasses the type's API.
- [ ] ASan/LSan, Valgrind, fuzz, or static-analysis memory findings are treated as review failures. Leaks, buffer overflows/out-of-bounds access, use-after-free, double-free, and uninitialized reads must be fixed at the root cause and reverified; suppressions or "known leak" quarantine do not satisfy this checklist.

## 9. TPM-specific

Cross-reference: `security.md` TPM-specific concerns.

- [ ] All TPM authorization values are `secret_buffer`. No `std::string` passwords sneak in via overloads.
- [ ] TSS parameter encryption (session encryption) is used for any sensitive parameter traveling between host and TPM, and the choice is documented at the call site.
- [ ] Software keys exist only in `secret_buffer` and only for the shortest possible window. Prefer TPM-bound keys when the operation allows.
- [ ] Persistent handles in the diff have documented ownership and eviction. Undocumented persistent-handle writes are a fail.
- [ ] TPM context ownership matches the documented thread-safety contract (cross-reference: `concurrency.md` TPM-specific concurrency).

## 10. Dependencies and CVEs

Cross-reference: `security.md` Dependencies and CVE tracking, `tpm-build-config` Dependency management.

- [ ] OpenSSL, TPM2 TSS, and optional logging dependencies such as spdlog are pinned. The diff does not bump to "latest" without a specific version.
- [ ] If a dependency is bumped, the changelog/security advisories for the new version were checked. The PR description names any CVEs being closed.
- [ ] If a new third-party dependency is added, the dependency-review section in the PR description is filled in (license, maintenance status, CVE history).
- [ ] Minimum patched dependency versions in the README still apply, or the README is updated in the same PR.

## 11. Build hardening

Cross-reference: `security.md` Build hardening, `tpm-build-config`.

- [ ] No release-build flag was relaxed (`-D_FORTIFY_SOURCE`, `-fstack-protector-strong`, `-fPIE`, `-fstack-clash-protection`, RELRO, NX). A relaxation requires explicit justification on the PR.
- [ ] No sanitizer was disabled in CI. The matrix (ASan, UBSan, TSan) still runs on the test suite.
- [ ] The sanitizer matrix is clean. Any ASan/LSan, UBSan, or TSan finding is a review failure, including findings in integration paths and findings believed to come from third-party code. Resolve the root cause through code, configuration, lifecycle isolation, or dependency update; do not pass the review with a skip, suppression, quarantine, retry, or "pre-existing" note.
- [ ] Security-checking tests are clean. Secret-leak sweeps, zeroization tests, oracle-uniformity checks, allowlist checks, RNG checks, session-encryption checks, and TPM authorization checks must pass; failed checks block the review until fixed and rerun.
- [ ] If a new compiler is added to the matrix, the warning set still rejects the same warnings.

## 12. Threat model adherence

Cross-reference: `security.md` Threat model.

- [ ] The change does not silently expand the in-scope threat model (e.g., new public API that promises kernel-level attacker resistance the library does not deliver).
- [ ] The change does not silently *contract* the threat model either (e.g., a new path that bypasses input validation because "internal callers can be trusted" — the public API is not internal).
- [ ] If the change affects the documented threat model, the README/CHANGELOG/ADR is updated in the same PR.

## Outcome

Produce a single PR comment in this shape:

```markdown
### Security review — tpm-security-review

| Section | Result | Notes |
| --- | --- | --- |
| 1. Secret handling | pass / fail | <file:line> — <justification> |
| 2. Constant-time | pass / fail | ... |
| ... | ... | ... |

Fails: <count>. N/A: <count> (reasons listed above).
```

A review with one or more **fails** blocks merge. A reviewer who marks an item **N/A** must give a one-line reason; "doesn't apply" without justification is treated as a fail.

## Error Handling

* **Reviewer marks an item N/A without a one-line reason.** Treat as **fail**. "Doesn't apply" without justification eliminates the audit trail the review exists to produce. Ask for the reason and re-evaluate.
* **Diff is ambiguous about whether a value carries a secret.** Default to **fail** with location and ask the author to either retype it as `secret_buffer` or document why the value is not secret-bearing. The cost of asking is small; the cost of a secret leaking through `std::vector<uint8_t>` is large.
* **New algorithm appears in the diff without justification.** Fail with a request for an ADR or inline justification referencing `security.md` Cryptographic algorithm choices. Do not approve "we can document it later" — the justification is the documentation.
* **Dependency bumped without a security review.** Fail. The PR must include the dependency-review block per `tpm-build-config` Dependency management (license, maintenance status, CVE history). Block merge until provided.
* **CVE-fixing dependency bump is missing from the CHANGELOG.** Fail. The CHANGELOG `### Security` entry must reference the CVE. Cross-reference `tpm-release` Step 3 and `tpm-write-docs` CHANGELOG format.
* **Security failure path leaks an oracle in error messages or logs.** Fail. The public-API outcome is a single `security_failure` with no caller-visible detail (`error-handling.md` Security-sensitive failures); the log record uses the coarse `source` value documented in `tpm-write-logging` `references/event-schema.md` Forbidden combinations.
* **ASan/LSan, UBSan, TSan, Valgrind, fuzzing, static analysis, or any security-check test reports a finding.** Fail. Memory leaks, buffer overflows/out-of-bounds access, use-after-free, double-free, uninitialized reads, undefined behavior, integer overflow, data races, secret leaks, oracle leaks, failed zeroization, failed allowlist checks, failed RNG checks, failed session-encryption checks, and failed TPM authorization checks cannot be skipped, suppressed, quarantined, downgraded, retried away, or waived. Fix the root cause and rerun the relevant check before the review can pass.
* **A finding is described as pre-existing, third-party, external, flaky, or unrelated to the diff.** Still fail. The review cannot pass while required security or sanitizer checks are failing; isolate the dependency path, update the dependency, change the test configuration to avoid unreviewable external code while preserving equivalent coverage, or fix the project code that triggers the finding.
* **Generic `/security-review` and this skill disagree on a finding.** Both findings stand. The generic command catches OWASP-style issues; this skill catches project-specific rules. Resolve each independently — neither overrides the other.
* **Review found one or more fails.** The PR comment lists every fail with `file:line` and a short justification, plus the N/A count and reasons. **A review with one or more fails blocks merge** until each is resolved or downgraded with a documented reason.

## Cross-references

- `security.md` — every section above maps to a section there. This skill is the procedure; that file is the policy.
- `error-handling.md` — error categories, oracle prevention, secret-derived bytes never in error messages.
- `logging.md` — never-log content, hot-path discipline.
- `concurrency.md` — TPM context ownership, OpenSSL threading.
- `tpm-write-code` — where secrets are typed (`secret_buffer`), where ports translate errors.
- `tpm-write-tests` — `PrintTo` redaction, recording-adapter pattern for log-content tests.
- `tpm-build-config` — sanitizer matrix, hardening flags, vcpkg pinning.
- `tpm-commit-pr` — the *Security impact* PR field that triggers this review.
