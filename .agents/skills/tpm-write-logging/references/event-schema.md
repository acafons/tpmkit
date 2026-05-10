# Standard Event Schema — tpmkit

This file defines the events the tpmkit library emits and the fields each event carries. It is the public contract for log consumers; renaming or removing an event is an API break (cross-reference `library-api-design.md` Versioning, `tpm-write-logging` Evolving the event schema).

## Structure

Every record contains:

- A **level** (`trace` / `debug` / `info` / `warn` / `error`).
- A **message** — short, human-readable, fixed wording per call site (no interpolation of variable data).
- A **field set** — `key=value` pairs from the field reference below.

Variable data lives in fields, not in the message. Two records that differ only in their field values share the same message text.

## Field reference

These fields appear across multiple events. Their type and semantics are fixed.

| Field | Type | Description |
|---|---|---|
| `event` | `snake_case` event name | Always required. Format: `<category>.<action>`. |
| `component` | adapter or composition unit | The folder name under `src/adapters/` (e.g., `tpm2_esys`, `tpm2_fapi`, `openssl`, `spdlog`), or `composition` / `domain` for non-adapter origins. |
| `outcome` | `success` \| `failure` | Required on terminal log lines for an operation. Lifecycle and informational events without a clear outcome may omit. |
| `error_category` | `input_error` \| `security_failure` \| `resource_error` \| `backend_error` | Required when `outcome=failure`. Mirrors the four categories from `error-handling.md`. |
| `error_code` | hex string of an adapter-internal numeric code | Adapter-boundary failure logs only. Original `TSS2_RC` (e.g., `0x0000098e`) or OpenSSL error code. Never appears on domain log lines. |
| `request_id` | opaque caller-supplied identifier | Optional; carried through context for correlation across calls. Must not come from a thread-local global (cross-reference `concurrency.md` Thread-local state). |
| `source` | `class::method` form (e.g., `esys_session_provider::open`) | Optional on most events; **forbidden on `outcome=failure` + `error_category=security_failure`** records (see *Forbidden combinations* below). The class and method that emitted the record. Format is `class::method` only — never `file:line`, never pointer values, never `this` addresses, never stack traces. |
| `algorithm` | algorithm identifier (`aes_256_gcm`, `ecdsa_p256`, `sha_256`, ...) | Optional; only when the algorithm is not itself a secret of the protocol. |
| `handle_kind` | `transient` \| `persistent` \| `session` | Used in place of the raw handle value, which is never logged. |
| `session_kind` | `hmac` \| `policy` \| `password` | TPM session classification when relevant. |
| `key_kind` | `transient` \| `persistent` \| `software` | Origin of a key referenced by an event. |
| `backend` | `fapi` \| `esys` \| `software` \| `mock` | Used at composition events. |
| `duration_ms` | non-negative integer | Optional; operation duration in whole milliseconds. Only when measured from inside the library. |
| `attempt` | positive integer | Used on retry events. |
| `reason` | short `snake_case` token | A *category* of reason (`hardware_busy`, `not_supported`, `insufficient_resources`), never a free-form message and never secret-derived. |

### Field stability

- Adding a new optional field — minor version bump.
- Adding a required field to an existing event — major version bump.
- Renaming or removing a field — major version bump.
- Changing the type of a field — major version bump (prefer adding a new field with a new type and deprecating the old).

## Events

Events are grouped by category. Categories may be added; see *Reserved categories* below.

### `tpm.*` — TPM lifecycle and operations

| Event | Level | Required fields | Optional fields |
|---|---|---|---|
| `tpm.session_open` | info | `event`, `component`, `outcome`, `session_kind` | `request_id`, `duration_ms` |
| `tpm.session_close` | info | `event`, `component`, `outcome`, `session_kind` | `request_id` |
| `tpm.key_load` | info | `event`, `component`, `outcome`, `key_kind` | `algorithm`, `request_id`, `duration_ms` |
| `tpm.key_evict` | info | `event`, `component`, `outcome`, `key_kind` | `request_id` |
| `tpm.persistent_handle_define` | info | `event`, `component`, `outcome`, `handle_kind` | `request_id` |
| `tpm.persistent_handle_evict` | info | `event`, `component`, `outcome`, `handle_kind` | `request_id` |
| `tpm.command_failed` | error | `event`, `component`, `outcome=failure`, `error_category` | `request_id`, `attempt`, `reason` |
| `tpm.command_retried` | warn | `event`, `component`, `outcome`, `attempt`, `reason` | `request_id` |
| `tpm.backend_error` | error | `event`, `component`, `outcome=failure`, `error_category`, `error_code` | `request_id`, `reason` |

Notes:

- `tpm.command_failed` is the operation-level failure as the caller saw it; `tpm.backend_error` is the adapter-boundary failure that *led to* the operation failure. Both may fire for a single failed call — the first names the domain error category, the second carries the third-party numeric code.

### `crypto.*` — cryptographic operations (host-side, OpenSSL)

| Event | Level | Required fields | Optional fields |
|---|---|---|---|
| `crypto.operation_failed` | error | `event`, `component`, `outcome=failure`, `error_category` | `algorithm`, `request_id` |
| `crypto.backend_error` | error | `event`, `component`, `outcome=failure`, `error_category`, `error_code` | `algorithm`, `request_id`, `reason` |
| `crypto.fallback_selected` | warn | `event`, `component`, `outcome=success`, `algorithm`, `reason` | `request_id` |

Notes:

- `crypto.*` covers OpenSSL-side operations. TPM-bound operations are under `tpm.*`.
- Successful crypto operations do **not** emit a per-call completion event. That would be hot-path traffic. Diagnose throughput with a profiler, not the log stream.

### `composition.*` — startup and wiring

| Event | Level | Required fields | Optional fields |
|---|---|---|---|
| `composition.backend_selected` | info | `event`, `component=composition`, `outcome=success`, `backend` | `reason` |
| `composition.config_applied` | info | `event`, `component=composition`, `outcome=success` | `reason` |
| `composition.startup_failed` | error | `event`, `component=composition`, `outcome=failure`, `error_category` | `reason` |

### `adapter.*` — generic adapter lifecycle

| Event | Level | Required fields | Optional fields |
|---|---|---|---|
| `adapter.fallback_selected` | warn | `event`, `component`, `outcome=success`, `reason` | `request_id` |
| `adapter.degraded` | warn | `event`, `component`, `outcome=success`, `reason` | `request_id` |

Use `adapter.*` only when the event is generic across adapter kinds. Adapter-specific lifecycle goes under the matching domain category (`tpm.*`, `crypto.*`).

## Reserved categories

The following category prefixes are reserved for future use; do not introduce events under them without first proposing a category extension via ADR (cross-reference `tpm-write-docs` ADR format).

- `policy.*` — TPM policy evaluation events.
- `attestation.*` — quote and certify operations.
- `nv.*` — NV index operations.

## Versioning the schema

Events and fields are part of the public surface. The full stability contract:

- **Add an event** — minor bump. Document in CHANGELOG `### Added`.
- **Add an optional field** to an existing event — minor bump. Document in CHANGELOG `### Added`.
- **Add a required field** to an existing event — major bump. Document in CHANGELOG `### Changed`.
- **Rename an event or field** — major bump. Prefer a deprecation cycle: add the new name in a minor release, mark the old as deprecated in this file, remove the old in the next major.
- **Remove an event or field** — major bump. Mark deprecated in a minor release first.
- **Change the type of a field** — major bump. Prefer adding a new field with a new type and deprecating the old.

The schema's own version is the library's version; there is no separate schema version.

## Forbidden in any event

These never appear in any record at any level (cross-reference `security.md` Logging):

- Key material, plaintext, ciphertext, MAC tags, signature bytes, authorization values, PINs, passphrases, derived secrets.
- Raw TPM handle values (use `handle_kind` instead).
- Pointer values that could leak ASLR offsets.
- File paths derived from caller input.
- Hostnames, usernames, IPs, or other identifiers the consumer may treat as PII.
- Free-form `reason` strings — `reason` is a `snake_case` token from a closed set.

If a candidate field would carry any of the above in some configuration but not others, the field is forbidden — the safe-by-default rule wins.

## Forbidden combinations

A few field combinations are forbidden because they would reintroduce a leak that other rules deliberately block.

### `source` on security failures

Records where `outcome=failure` and `error_category=security_failure` must **not** carry a precise `source` value distinguishing which check failed (e.g., `source=mac_validator::verify` versus `source=padding_validator::verify`). The oracle-prevention rule in `error-handling.md` (Security-sensitive failures) and `security.md` (Failure handling) requires that a security failure surface only "verification failed" with no caller-visible detail beyond the category. Logs flow to SIEM, third-party shippers, and ops teams in real deployments — assuming the log stream is fully trusted is risky, and a precise `source` would rebuild the oracle the rule was meant to block.

For `security_failure` records, choose one of:

- **Omit `source` entirely** (preferred — the safest default).
- **Set `source` to a deliberately coarse common value** that does not distinguish sub-checks: `source=verifier::check`, `source=adapter::verify`. This is acceptable when omission would lose useful coarse attribution (e.g., distinguishing the `tpm2_esys` verifier from the `openssl` verifier — but only at that granularity).

For all other failure categories (`input_error`, `resource_error`, `backend_error`) and all success records, `source` is unconstrained — the precise `class::method` form is fine.

This is the only currently-defined forbidden combination. If a new event introduces a similar oracle risk, document the combination here rather than encoding it implicitly in call-site code.
