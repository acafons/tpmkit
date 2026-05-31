# C++17 Logging Rules

These rules build on `security.md` (what must never be logged), `library-api-design.md` (the `logger` is a domain port), and `error-handling.md` (logging at adapter boundaries before translation). This file owns the logger port's *shape* and the discipline around using it.

## The logger is a port

The library does not import a logging framework. The domain declares an abstract `logger` port; consumers wire an adapter (spdlog, syslog, `std::cerr`, no-op) at the composition root. The library must function correctly with logging fully disabled. This follows directly from `architecture.md`: anything backend-specific belongs behind a port.

## Logger ownership

One composition root selects one logger for the object graph it owns. Components
constructed from an existing owner or context inherit that owner's logger; they
do not become a fresh logging composition root.

For example, a context member factory such as
`tpm_context::create_pcr_provider()` must borrow the effective logger already
owned by that context. It must not expose another logger parameter, create a
second no-op fallback, or let callers split one TPM connection's audit trail
across different logger instances.

Adapters constructed directly by a composition root take `logger&` at
construction, with the no-op default handled at that boundary. Public
configuration/factory APIs may accept a nullable `std::shared_ptr<logger>` for
caller ergonomics, but the value is normalized exactly once at construction:
`nullptr` becomes `noop_logger`, and no component stores or branches on a null
logger afterward. Adapter constructors and context-derived factories do not
accept nullable logger inputs. Nullable logger pointers are limited to private
helper functions used while the owning context is still being created or while
translating backend errors at an already-null-tolerant boundary.

## Port shape

The `logger` port has exactly one logging method, taking a structured record. It does not take a preformatted string. Adapters decide formatting; the domain decides content.

```cpp
namespace tpmkit {

enum class log_level { trace, debug, info, warn, error };

struct log_field {
    std::string_view key;
    std::string_view value;       // adapter formats; library does not
};

class logger {
public:
    virtual ~logger() = default;

    // Thread-safe, no-throw. Adapters that wrap a non-thread-safe
    // sink must serialize internally. A failure to write is swallowed
    // by the adapter and never propagated to the caller.
    virtual void log(log_level level,
                     std::string_view message,
                     gsl::span<const log_field> fields) noexcept = 0;
};

} // namespace tpmkit
```

Notes on the shape:

- **One method, not five.** A single `log(level, ...)` method has one place to filter and one place to test. Per-level methods (`info`, `error`, ...) duplicate the surface, complicate mocking, and offer nothing the call-site macros (below) cannot provide.
- **`message` is human text, not event identity.** Production call sites pass fixed, English, human-readable wording such as `"PCR extend completed"`. The stable machine-readable identity belongs in the structured `event` field, such as `event=tpm.pcr.extend_completed`. Do not use the event name as the message in production records.
- **`std::string_view` for keys and values.** The port must not retain pointers to caller buffers across calls. Adapters that need to persist a record copy it before returning from `log`.
- **`gsl::span` for the field list.** A non-owning view of a caller-provided array. Cross-reference: `code-standards.md` Memory and resources, `security.md` Memory safety.
- **No `format` argument.** The library never asks the adapter to do printf-style substitution; it builds the message and the fields itself. This is the only way to keep secrets out of the format pipeline reliably.
- **`noexcept`.** A logger that throws would couple every call site to exception handling; not a tradeoff worth making for a side-channel like logs.

## Levels

- **`trace`** — call-flow detail, off in release builds by default. Used to trace which adapter took which branch. Never used on a hot path.
- **`debug`** — slow-path diagnostics. State transitions, retry attempts, fallback selection. Off in release builds by default.
- **`info`** — lifecycle events: TPM session created or closed, key loaded or evicted, configuration applied. One entry per significant state change, not per operation.
- **`warn`** — degraded but continued. Soft failures, retries, fallbacks that succeeded.
- **`error`** — operation failed. Always paired with the failure being surfaced to the caller via `outcome<T, error>` or an exception — never a substitute for returning the error.

There is no `fatal` level. The library does not abort the process; that is the consumer's decision. A condition severe enough to abort still goes through `error` and a returned/thrown failure.

## What to log, by level

| Level | Examples |
|---|---|
| `error` | Translation point: an OpenSSL or TSS2 call returned non-success. Log original error code at the adapter, then translate. |
| `warn` | TPM busy, retried successfully. Falling back from FAPI to ESYS. Configuration value out of range, default applied (when allowed by `security.md`). |
| `info` | TPM session opened with attributes X. Key loaded at handle Y. Persistent handle Z evicted. Backend selection at composition root. |
| `debug` | Adapter chose path A over B because of capability check. Request size, algorithm chosen (when not secret). |
| `trace` | Entry/exit of selected functions during integration debugging. Off by default everywhere. |

## What never to log

Cross-reference: `security.md` Logging — the never-log list is owned there. Restating the headline: no key material, plaintexts, ciphertexts, signatures, MACs, authorization values, PINs, passphrases, derived secrets, or pointer values that leak ASLR.

If a structured field's *key* is sensitive (e.g., a session-correlation token), use a hashed or truncated form, not the raw value. If the *value* is sensitive, omit the field entirely — there is no "but it would be useful to debug" exception.

## Structured fields

- **Key naming.** Keys are `snake_case`, stable, and documented per call site. Renaming a field is an observable change for log consumers and counts as a minor version bump.
- **Value formatting.** Adapters format. The library passes already-stringified values (`std::to_string`, hex for byte counts, etc.); it does not pass binary blobs.
- **No timestamps.** Adapters add timestamps. A library-emitted timestamp is wrong by the time the adapter writes it, and consumers may want a different clock.
- **No PII or system info.** No usernames, hostnames, IPs, file paths derived from caller input. The library does not know what the consumer considers sensitive.
- **No correlation IDs from globals.** If a caller wants correlation, they pass a context object that carries the ID into each call site explicitly. Thread-local correlation IDs are forbidden (cross-reference: `concurrency.md` Thread-local state).

## Filtering

- **Runtime filtering** is the primary mechanism. Adapters apply the active level at call time so consumers can adjust dynamically without recompiling.
- **Compile-time ceiling.** `TPMKIT_LOG_MAX_LEVEL` elides call sites *above* a threshold at preprocessing. Release builds default to a ceiling of `info`, so `debug` and `trace` paths are not compiled into release binaries by default. This is a backstop — the never-log rules in `security.md` are the primary enforcement.
- **No global level setter** in the library. The library logs at the level it considers appropriate; the adapter filters. Consumers who want to flip a level do so on their adapter.

## Hot-path discipline

- Crypto inner loops, per-byte processing, and constant-time code never log at any level (cross-reference: `security.md` Logging, `performance.md` Hot-path style).
- Use compile-time-elided macros at the call site so a `TRACE` line in a hot function does not compile to a function call in a release build:

  ```cpp
  #if TPMKIT_LOG_MAX_LEVEL >= TPMKIT_LOG_TRACE
  #  define TPMKIT_LOG_TRACE(logger, msg, ...) /* call */
  #else
  #  define TPMKIT_LOG_TRACE(logger, msg, ...) ((void)0)
  #endif
  ```

  The macros are project-internal; the public port is the `logger` class above.
- Do not build a structured field array on a hot path *only* to discard it under filtering. Guard the construction with the same compile-time check that gates the call.

## Adapters and defaults

- **No-op adapter** is the library default when no logger is wired. A public
  config value of `nullptr` selects `noop_logger` at the API boundary.
  `noop_logger::log` is empty and `noexcept`. The library compiles and runs
  identically with logging disabled.
- **`noop_logger::instance()` is allowed as a Null Object convenience.** It returns a stateless, immutable no-op logger reference; it is not a configurable global logger and must not be used to smuggle mutable logging state into the library.
- **Reference adapter for spdlog** lives under `src/adapters/logging/spdlog/`. It demonstrates structured-field translation, level mapping, and the never-retain-pointers contract.
- **stdio adapter** under `src/adapters/logging/stdio/` is the zero-dependency reference for tests and one-binary tools.
- **Recording adapter** under `src/adapters/mock/` captures records into an in-memory vector for test assertions.

## Testing

- **Test on observable behavior, not log lines.** Asserting that a specific log message appears couples the test to wording that will drift. Log assertions are a smell; they belong in tests *of the adapter*, not of the domain.
- **The recording adapter** is for validating that the right *level* and *field keys* are emitted at lifecycle boundaries — never the message text.
- **Coverage of the never-log rules** is enforced by the recording adapter plus a test that scans recorded values for known-secret patterns and fails if any appear. Cross-reference: `tpm-write-tests`.
- **Test with the no-op adapter** at least once per port test suite. If a test passes only with logging on, the test is depending on side effects that should not exist.

## When in doubt

- Prefer fewer log lines that say something useful over many lines that say "entered function." Modern profilers, sanitizers, and debuggers replace most of what tracing was historically used for.
- A line that would be redacted by every reasonable consumer should not be emitted in the first place. The library is responsible for not generating sensitive log content; the adapter is responsible for not formatting it badly.

## Where the implementation rules live

This rule defines the port shape and the never-log policy. The procedural side — the **standard event schema** (event names, field names, required versus optional fields, stability contract), the procedure for **adding a log call site**, and the mechanics for **implementing a logger adapter** (spdlog default, recording adapter for tests) — lives in the `tpm-write-logging` skill. The schema is the public surface log consumers query against, so it is treated as a contract, not as prose.
