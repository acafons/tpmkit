# C++17 Error Handling Rules

These rules build on `code-standards.md` (Error handling) and `library-api-design.md` (exception safety). They define how errors are categorized, surfaced, and translated between layers.

## Error categories

The library defines a small, closed set of domain error categories. Every adapter error and every public-API error must map to one of:

- **`input_error`** — caller supplied invalid arguments (bad length, unknown algorithm, malformed data). Recoverable; the caller can retry with corrected input.
- **`security_failure`** — verification failed, MAC mismatch, signature invalid, policy not satisfied. Not recoverable in place; the operation is rejected.
- **`resource_error`** — TPM unavailable, file not found, allocation failed, session exhausted. Possibly recoverable by retry or fallback.
- **`backend_error`** — unexpected error from a third-party library. Domain code treats this as opaque; the adapter logs the underlying detail before translating.

Add new categories only with documented justification. Avoid an `unknown` or `other` catch-all.

## The error type

The `error` value used by `outcome<T, error>` carries exactly two things:

- **A category** — one of the four values above. Mandatory. The only programmatic-dispatch surface callers should branch on.
- **A message** — human-readable, free-form. For logs and display, not for programmatic branching. Must be generic for `security_failure` (no oracle-leaking detail).

The `error` type does **not** carry:

- Numeric codes from third-party libraries (`TSS2_RC`, OpenSSL error stack, `errno`). Logged at the adapter boundary and dropped.
- A library-specific numeric code alongside the category. The category enum is the discriminator — adding a code recreates `errno` with no extra signal.
- Adapter identity. Callers must not be able to tell whether `resource_error` came from FAPI, ESYS, or the software fallback.

If a single operation needs more granular programmatic dispatch than the four categories provide, give that operation a more specific return type — do not extend the universal `error` type.

## Throw vs. return

- **Throw** for genuinely exceptional conditions: programmer errors, allocation failure, contract violations.
- **Return** an `outcome<T>` (or `std::optional<T>` for binary success/failure) for **expected** failures: invalid input, missing keys, signature mismatch. A failed crypto verification is a normal outcome, not an exception.
- C++17 has no `std::expected`. Use `tl::expected` (header-only backport) or a small in-house `outcome<T, error>` built on `std::variant`. Pick one and apply it consistently across the project.
- Never throw across an `extern "C"` boundary or a callback registered with a C library. Catch at the boundary and translate to an error code.

## Exception types

- All exception types derive from a single base, `tpmkit_error`, which derives from `std::runtime_error`.
- Names describe the failure mode, not the class that threw — `invalid_argument_error`, `precondition_error`, `allocation_error`. Do not name exceptions after the class that throws them; that fragments the hierarchy and duplicates category information already carried by `outcome<T>`.
- Use the `_error` suffix consistently. The standard library is mixed (`std::runtime_error` vs. `std::invalid_argument`); we pick the suffixed form for predictability.
- New exception types require justification. There should be far fewer exception types than classes that can throw, because most failures go through `outcome<T>` instead.

## Translating third-party errors

- Adapters own the translation from `TSS2_RC`, OpenSSL error stacks, and OS errno values into domain errors. Domain code never sees a third-party error type.
- The original error code and message are logged at the adapter boundary, then dropped. Do not propagate raw third-party errors into domain types.
- Translation tables (e.g., `TSS2_RC` → category) live next to the adapter, not in the domain.
- A third-party call returning an error with no obvious mapping is classified as `backend_error`. The adapter's log line must contain enough detail to diagnose offline.

## Security-sensitive failures

- Security failures (signature, MAC, policy) return a single `security_failure` outcome with no caller-visible detail beyond the category. Detailed reasons are logged internally only. This blocks oracle-style attacks that distinguish "wrong padding" from "wrong MAC."
- Never include secret-derived bytes in any error message, even internal logs. Cross-reference: `security.md` Logging.

## Exception safety guarantees

- Public functions document one of: `noexcept`, **strong** (operation rolls back on failure), **basic** (no leaks, valid state, partial effects allowed), or **none** (rare; document why).
- Move operations and destructors are always `noexcept`.
- Crypto and TPM operations default to **strong** when feasible. When not feasible (e.g., a session has already been consumed), the function documents the partial-effect state explicitly.

## No silent failure

- Every error path either returns to the caller, throws, or is logged before being intentionally swallowed at a documented boundary.
- A function that fails must never return a default-constructed result that could be mistaken for success. Use `outcome<T>` to make the failure explicit in the type.
- `catch (...)` is allowed only at thread entry points and the `extern "C"` boundary. It must log the unknown exception before continuing.
