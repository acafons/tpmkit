# C++17 Security Rules

These rules apply to the entire library. Crypto and TPM code is security-critical by default — assume every line will be audited.

## Threat model

- **In scope:** API misuse by callers, accidental exposure of secrets in memory or logs, timing oracles in equality checks, integer overflow in size arithmetic, untrusted input at the public API boundary.
- **Out of scope:** kernel-level attackers, physical attacks on the TPM or host, side channels inside dependencies (OpenSSL, TSS).
- The library never assumes the host process is trusted. Secrets must be cleared as soon as they are no longer needed.

## Secret handling

- Define a `secret_buffer` (or `secret_string`) type that owns sensitive bytes and zeroes them in its destructor using `OPENSSL_cleanse` (or an equivalent that the compiler cannot elide). All key material, passphrases, PINs, HMAC keys, and TPM authorization values must use this type — never raw `std::string` or `std::vector<uint8_t>`.
- `secret_buffer` must be move-only. Deleted copy operations prevent silent fan-out of secrets.
- Lock secret pages into RAM (`mlock` / `VirtualLock`) where the platform supports it, and unlock before zeroing on destruction.
- Never let a secret reach `std::cout`, log sinks, exception messages, or `to_string()`-style helpers. Secret types must not have streaming operators.
- Treat process core dumps as exposure. Disable core dumps for processes handling raw key material when possible, or document the requirement for consumers.

## Constant-time operations

- Use `CRYPTO_memcmp` (OpenSSL) for any comparison where one side is or is derived from a secret: MAC tags, signature bytes, password equality, capability tokens. Never `memcmp`, `==`, or `std::equal`.
- Avoid secret-dependent branches and secret-indexed table lookups in code we write. Delegate primitives to OpenSSL and TSS rather than implementing them.

## Randomness

- Never roll a custom RNG. Use OS-provided randomness (`getrandom`, `BCryptGenRandom`) or `RAND_bytes` from OpenSSL.
- For TPM-bound operations that require randomness inside the TPM trust boundary, use `Esys_GetRandom` / FAPI equivalents — not host RNG.
- Treat every nonce as having uniqueness requirements. Document them at the call site of any function that takes a nonce.
- Never seed any RNG from predictable sources (PID, time, hostname).

## Input validation at the public API boundary

- Validate every parameter at the public API. Trust internal callers; do not re-validate within the domain.
- Reject unknown algorithm identifiers, OIDs, and curve names rather than falling through to a default.
- Check size and length fields before any allocation derived from them. Guard against integer overflow when multiplying or adding sizes — use checked arithmetic helpers, not raw `+`/`*`.
- Initialize every stack buffer (`std::array<uint8_t, N> buf{};`). Never declare an uninitialized byte buffer.
- Use `gsl::span` (or an internal `byte_span` type — `std::span` is C++20, not available in C++17) or bounds-checked accessors (`.at()`) for any buffer derived from caller input.

## Cryptographic algorithm choices

- Allowed by default: AES-GCM (256-bit), ChaCha20-Poly1305, ECDSA on P-256/P-384, Ed25519, RSA-PSS with 3072-bit minimum, SHA-256/SHA-384/SHA-512, HKDF.
- Forbidden for security uses: MD5, SHA-1, RC4, DES/3DES, AES-ECB, RSA-PKCS#1 v1.5 signing, raw ECB modes.
- SHA-1 is allowed only for legacy TPM PCR bank compatibility and is disabled
  by default. Any API path that accepts or returns SHA-1 PCR data must be gated
  behind `TPMKIT_ENABLE_LEGACY_SHA1_PCR`; default builds reject or ignore the
  legacy bank before exposing it as supported output.
- Algorithm selection is always explicit at the public API. Do not introduce silent defaults that can change between versions.
- New algorithms require a documented justification before they can appear in the public API.

## Failure handling

- Fail closed. Any error in a security-relevant check (signature verification, MAC validation, policy evaluation) rejects the operation outright. Never proceed on a "best effort" basis.
- Do not return detailed error information across the public API for security-sensitive failures — surface a single "verification failed" outcome and log details internally. This blocks oracle-style attacks.
- Never silently suppress an error from OpenSSL or TSS. Translate it into a domain error at the adapter boundary; do not drop the error code.

## Logging

- The library does not impose a logging framework. The domain declares an abstract `logger` port; consumers wire an adapter (spdlog, syslog, std::cerr, no-op) at the composition root. The library must function correctly with logging fully disabled.
- The `logger` port is thread-safe and no-throw by contract. Adapters wrapping a non-thread-safe logger must serialize internally; adapters that fail to write a record must swallow the failure rather than propagate it.
- Records are structured (level, message, optional key/value pairs), not preformatted strings — adapters decide formatting. The port must not retain pointers to caller buffers across calls.
- **Never logged at any level (including `DEBUG` and `TRACE`):** key material, plaintexts, ciphertexts derived from caller secrets, signatures over caller data, MAC tags, TPM authorization values, PINs, passphrases, session encryption keys, derived secrets, pointer values that could leak ASLR offsets, and stack traces that include argument values of secret-typed parameters.
- **Logged at adapter boundaries:** errors from OpenSSL and TSS, before translation to domain errors. Include the original error code and library context, but never secret-derived bytes (no "expected MAC X, got Y").
- **Log levels:** `ERROR` (operation failed), `WARN` (degraded continued), `INFO` (lifecycle events: TPM session created/closed, key loaded, persistent handle evicted), `DEBUG` (slow-path diagnostics), `TRACE` (call-flow detail; off by default). Hot paths (per-byte crypto loops, inner loops of constant-time code) never log at any level. The library never sets a global level — it logs at the level it considers appropriate and lets the adapter filter.
- **Filtering:** runtime filtering is the primary mechanism — adapters apply the active level at call time, and consumers can adjust dynamically without recompilation. An optional compile-time ceiling (`TPMKIT_LOG_MAX_LEVEL`) elides call sites *above* a threshold; below the ceiling, runtime filtering applies. Release builds default to a ceiling of `INFO`, so `DEBUG` and `TRACE` paths are not compiled into release binaries by default. This is a backstop for the never-log rules above, not their primary enforcement — the rules forbid writing those paths in the first place.

## Memory safety

- No raw pointer arithmetic on caller-supplied buffers. Wrap them in `gsl::span` (or an internal `byte_span` type) immediately at the API boundary.
- No `reinterpret_cast` from external bytes to a structured type without prior length and alignment validation. Prefer explicit deserialization.
- Bounds-checked access (`.at()`) for any index derived from external input. `[]` only when the index is provably in range from local code.

## TPM-specific concerns

- Treat all TPM authorization values (passwords, HMAC keys, policy secrets) as `secret_buffer`.
- Use TSS parameter encryption (session encryption) for any sensitive parameters traveling between the host and the TPM. Document which sessions are encrypted at each call site.
- Prefer TPM-bound keys over software keys when the operation allows it. Software keys live in `secret_buffer` and exist for the shortest possible window.
- Be deliberate about persistent handles. Document who owns each persistent handle and when it is evicted.

## Dependencies and CVE tracking

- Pin OpenSSL, TPM2 TSS, and optional logging dependencies such as spdlog to
  specific versions in the build configuration and vcpkg manifest. Never depend
  on "latest."
- Track upstream security advisories for OpenSSL, TPM2 TSS, and spdlog.
  Bumping a dependency for a security fix takes priority over feature work.
- Document the minimum patched version of each dependency in the project README.

## Build hardening

- Enable `-D_FORTIFY_SOURCE=2`, `-fstack-protector-strong`, `-fPIE`, and `-fstack-clash-protection` for release builds. RELRO and NX are required on platforms that support them.
- CI runs ASan, UBSan, and TSan on the test suite. A sanitizer failure is a release blocker.
- Detailed flag definitions live in `build-and-tooling.md` once it exists; this file owns the policy that those flags must be enabled.
