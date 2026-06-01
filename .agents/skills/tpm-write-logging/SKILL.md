---
name: tpm-write-logging
description: Logging implementation guide for the tpmkit C++17 library — implementing a logger adapter (spdlog default), choosing event names and fields when adding log call sites, the standard event schema (event names, field names, required versus optional), and the stability contract that makes events part of the public surface. Use when adding log call sites in domain or adapter code, implementing a new logger adapter, evolving the event schema, or wiring a logger at the composition root. Do not use for adapter-error translation, the never-log security rules, or general port-design questions.
---

# Writing Logging — tpmkit

This skill covers the *implementation* side of logging in tpmkit. The always-on rule `logging.md` defines the port shape, levels, and never-log rules. This skill defines:

- the **standard event schema** (event names, fields, stability contract — in `references/event-schema.md`),
- the **procedure for adding a log call site**,
- the **mechanics for implementing a logger adapter**, with **spdlog** as the default reference.

The reason this is a skill and not a rule: the schema is the public surface that downstream log consumers query against. Adding a new event or renaming a field is a deliberate operation that needs a procedure, not just a policy. Bury the schema in a rule and it gets edited like prose; surface it in a skill and it gets touched like a contract.

## Default adapter: spdlog

The reference logger adapter uses **spdlog**. It is mature, structured-logging-capable, asynchronous-sink-capable, widely audited, and stable on the supported toolchains. The choice does not bind consumers — they wire whatever they want behind the `logger` port — but it is what the project's binaries, examples, and tests use by default.

Constraints that follow from `logging.md` and `security.md`:

- Production spdlog includes live only under `src/adapters/logging/spdlog/`. Grep should find zero `#include <spdlog/` matches under `include/` and under `src/` outside that folder; adapter tests and downstream smoke fixtures may include spdlog directly.
- Production logging adapters live under `src/adapters/logging/<backend>/`, with matching unit tests under `tests/unit/logging/<backend>/`.
- The spdlog version is pinned in the vcpkg manifest with a documented minimum (cross-reference `tpm-build-config` Dependency management). Bump deliberately when an upstream advisory or a needed bug fix lands.
- The adapter implements `tpmkit::logger` exactly as defined in `logging.md`: thread-safe, `noexcept`, never retains pointers across calls, swallows write failures.

## Owning the logger

The `logger` is a port like `key_provider` or `crypto_primitives`, but it is a **cross-cutting concern** — the operation's outcome does not depend on whether logging is enabled. The project's pattern is constructor injection of `logger& log` with a stateless no-op default (`noop_logger::instance()`), wired logger-first in the composition root so every adapter's reference outlives the adapter. Public configuration/factory APIs may accept a nullable `std::shared_ptr<logger>` only as a boundary convenience; normalize it immediately to an effective non-null logger (`nullptr` maps to `noop_logger`) and store/use the non-null logger thereafter. Components derived from an existing owner/context use a backend-neutral owner member factory (`ctx.create_*()`), borrow the owner's effective logger internally, and do not expose another logger override. Singletons, thread-locals, globals, setter injection, adapter-constructor nullable loggers, and per-method logger parameters are all forbidden.

Read `references/owning-the-logger.md` when wiring a new adapter or composition root, deciding how a class should take the logger, creating a public factory from an existing context, or reviewing a PR that touches logger ownership. It covers the where-logging-lives split (adapters and composition only), the constructor-with-no-op-default sketch, context-derived component wiring, why a stateless no-op is not the singleton anti-pattern, the composition-root wiring example with lifetime-ordering rules, and the full anti-pattern list.

## Adding a log call site

When you are about to write `logger_.log(...)` somewhere, walk through this:

1. **Decide the level.** Cross-reference `logging.md` Levels.
   - `error` — operation failed; paired with the failure being returned/thrown.
   - `warn` — degraded but continued (retried successfully, fallback selected).
   - `info` — lifecycle event (session opened, key loaded, backend selected).
   - `debug` — slow-path diagnostic.
   - `trace` — call-flow detail; off in release.
2. **Pick the event name.** Read `references/event-schema.md` and find the matching entry. The event name is `category.action` in `snake_case` (e.g., `tpm.session_open`, `composition.backend_selected`). If no entry fits, propose a new one — see *Evolving the event schema* below; **do not invent an ad-hoc name at the call site.**
3. **Determine the required fields** for that event from the schema. These are mandatory; omitting them violates the schema.
4. **Add optional fields** when they help diagnose without leaking. The schema lists candidates per event.
5. **Verify the never-log rules.** No key material, plaintext, ciphertext, MAC, signature, authorization value, PIN, passphrase, derived secret, raw handle value, or pointer (cross-reference `security.md` Logging, `logging.md` What never to log). When in doubt, omit the field.
6. **Build the field array on the stack** with `std::array<log_field, N>`. Pass as `gsl::span` to `log()`. Do not allocate on a hot path.

Implementation note for adapter-local event catalogs: keep each schema event name
coupled to its fixed human message in one descriptor object. For the TPM2 ESYS
adapter this is `events::event_descriptor` in
`src/adapters/tpm2_esys/support/log_events.h`. Logging helpers should accept the
descriptor and derive both the structured `event` field (`descriptor.name`) and
the logger message (`descriptor.message`) from it. Do not maintain parallel
`events::*` and `events::messages::*` constants, and do not pass message text and
event names as independent parameters at call sites.

### Content rules for messages and fields

Three rules apply on top of the never-log policy in `security.md` Logging:

- **English only.** Messages and field keys are written in English, aligning with `code-standards.md` ("All code must be written in English"). This keeps log streams searchable across regions and avoids encoding pitfalls in adapters that assume narrow ASCII or UTF-8 subsets.

- **Message text is fixed human text per call site — no event names, no format-string substitution.** Variable data goes in *fields*, never in the message. Event identity goes in the structured `event` field (`descriptor.name`), while the logger message is human-readable wording (`descriptor.message`). This is both a discipline rule (machines parse fields, not messages) and a security rule. Format-string injection is a real attack class — every layer that runs a runtime string through `printf`-family substitution becomes a place attacker-influenced bytes can corrupt the format. "Messages are string literals at compile time" forecloses that whole category by construction. `fmt::format` belongs *inside the adapter* on rendering, never at the call site.

- **Field values come from bounded sets or have bounded length.** Categorical values (`reason=hardware_busy`, `algorithm=ecdsa_p256`, `state_transition=opened_to_authenticated`) are best — they round-trip through queries cleanly and do not blow up log indexers. Numeric values with bounded magnitude (`attempt=2`, `duration_ms=42`) are fine. Free-form strings, raw nonces, randomly-generated identifiers, and other high-cardinality data create real cost on consumer infrastructure (one shard per unique value in many indexers) and usually overlap with the never-log list anyway. If a field looks high-cardinality, replace it with a bounded category (`size_bucket=medium` instead of `size=1024`) or omit it.

Example, lifecycle event for a successful session open:

```cpp
const std::array<log_field, 5> fields{{
    {"event",        "tpm.session_open"},
    {"component",    "tpm2_esys"},
    {"outcome",      "success"},
    {"session_kind", "hmac"},
    {"source",       "esys_session_provider::open"},
}};
logger_.log(log_level::info,
            "TPM session opened",
            gsl::span<const log_field>(fields));
```

Example, adapter-boundary failure log before translation:

```cpp
const std::array<log_field, 7> fields{{
    {"event",          "tpm.backend_error"},
    {"component",      "tpm2_esys"},
    {"outcome",        "failure"},
    {"error_category", "backend_error"},
    {"error_code",     to_hex(rc)},     // TSS2_RC, never a secret
    {"backend_error_description", decoded_backend_description},
    {"source",         "esys_session_provider::start_auth_session"},
}};
logger_.log(log_level::error,
            "Esys_StartAuthSession failed",
            gsl::span<const log_field>(fields));
```

The adapter logs *before* translating to a domain `error` (cross-reference `error-handling.md` Translating third-party errors). The original numeric code and sanitized decoded backend diagnostic text go here and nowhere else.

The `source` field carries the `class::method` that emitted the record so consumers can attribute root cause without grepping the source tree. **It is forbidden on `outcome=failure` records with `error_category=security_failure`** to preserve the oracle-prevention rule from `error-handling.md` (Security-sensitive failures) — see `references/event-schema.md` Forbidden combinations. On every other event, including `backend_error`, `resource_error`, `input_error`, and all success records, `source` is fine.

### What the records actually look like

The two call sites above produce records like the ones below. The **library** emits the message text and the structured fields; the **adapter** (here, spdlog with its default pattern) prefixes the timestamp, logger name, and level. The library never emits timestamps, hostnames, PIDs, or correlation IDs of its own (cross-reference `logging.md` Structured fields).

`key=value` format — what the spdlog sketch in the next section produces:

```
[2026-05-09 14:23:01.234] [tpmkit] [info]  TPM session opened event=tpm.session_open component=tpm2_esys outcome=success session_kind=hmac source=esys_session_provider::open
[2026-05-09 14:23:05.678] [tpmkit] [error] Esys_StartAuthSession failed event=tpm.backend_error component=tpm2_esys outcome=failure error_category=backend_error error_code=0x0000098e backend_error_description=esapi:try_again source=esys_session_provider::start_auth_session
```

JSON format — same call sites, different adapter rendering (spdlog with a JSON pattern, or a custom sink):

```json
{"ts":"2026-05-09T14:23:01.234Z","level":"info","msg":"TPM session opened","event":"tpm.session_open","component":"tpm2_esys","outcome":"success","session_kind":"hmac","source":"esys_session_provider::open"}
{"ts":"2026-05-09T14:23:05.678Z","level":"error","msg":"Esys_StartAuthSession failed","event":"tpm.backend_error","component":"tpm2_esys","outcome":"failure","error_category":"backend_error","error_code":"0x0000098e","backend_error_description":"esapi:try_again","source":"esys_session_provider::start_auth_session"}
```

Notes:

- **The choice between `key=value` and JSON is per-project, not per-call-site.** Pick one in this skill and commit; mixed-format streams break every downstream parser.
- **JSON is preferred** when the logs feed an ingestion pipeline (Elastic, Loki, CloudWatch, Splunk) or are consumed by audit tooling — every field is unambiguously typed and addressable. **`key=value` is preferred** when the logs are read by humans tailing a file in development.
- The same call site produces *both* renderings depending on which adapter is wired at the composition root. Two consumers picking different formats does not require any code change in the library.

### Compile-time elision for hot-path call sites

`logging.md` defines `TPMKIT_LOG_MAX_LEVEL` as a compile-time ceiling that elides call sites *above* the threshold at preprocessing — release builds default to `info`, so `debug` and `trace` paths are not compiled into release binaries by default. To benefit from the elision, hot-path call sites use the macro form rather than calling `logger_.log(...)` directly; the macro guards both the call and the field-array construction.

```cpp
#define TPMKIT_LOG_TRACE_(logger, message, ...)                       \
    do {                                                              \
        if constexpr (TPMKIT_LOG_MAX_LEVEL >= TPMKIT_LOG_LEVEL_TRACE) {\
            (logger).log(::tpmkit::log_level::trace,                  \
                         (message), { __VA_ARGS__ });                 \
        }                                                             \
    } while (false)
```

Used at a call site:

```cpp
TPMKIT_LOG_TRACE_(log_, "session retried",
    log_field{"event",     "tpm.command_retried"},
    log_field{"component", "tpm2_esys"},
    log_field{"outcome",   "success"},
    log_field{"attempt",   std::to_string(attempt)},
    log_field{"reason",    "hardware_busy"});
```

In a release build with `TPMKIT_LOG_MAX_LEVEL = info`, the entire `do { ... } while (false)` block compiles to nothing — the field-array construction is elided alongside the call, removing the cost on the hot path. In a debug build with the ceiling at `trace`, the call compiles in and the adapter's runtime filter applies on top.

Use the macros only on `debug` and `trace` call sites where elision matters. `info`, `warn`, and `error` calls go through `logger_.log(...)` directly — they are not on hot paths and do not benefit from compile-time elision.

### Identifying which thread emitted a record

Two distinct concerns get conflated under "thread identification" — they need different mechanisms, and picking the wrong one wastes hours of debugging.

| Question | Tool |
|---|---|
| Which OS thread serialised the corruption I'm chasing? | Adapter pattern with `%t` (or equivalent). |
| Which logical operation produced this audit-relevant sequence of events? | `request_id` field, threaded through call-site context. |
| Which adapter emitted this record? | The schema's required `component` field. |

#### OS thread ID — adapter responsibility

Useful when debugging a hang, a race, or a TPM/OpenSSL state-corruption symptom that turns out to be a concurrency bug. The library does **not** emit thread IDs (cross-reference `logging.md` Structured fields, `concurrency.md` Thread-local state) — that is part of the metadata adapters add, alongside timestamps. Configure the adapter's output pattern to include it.

`key=value` form with spdlog:

```cpp
sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [thread %t] %v");
```

Output (the same `tpm.session_open` call site as before, now with thread ID prepended by the adapter):

```
[2026-05-09 14:23:01.234] [tpmkit] [info]  [thread 7feab4002700] TPM session opened event=tpm.session_open component=tpm2_esys outcome=success session_kind=hmac
```

JSON form: add a `"thread":"%t"` field to the JSON pattern. The library does not need to know.

#### Logical request correlation — library responsibility

Useful when a single logical operation crosses thread boundaries — a request handler dispatches work to a pool, a background job cleans up after a foreground operation, or a long-running operation hops between an HMAC session thread and a key-loading thread. The OS thread ID is the wrong tool here; threads can rotate, pools can recycle, and the same thread services many requests. You want an identifier that follows the *operation*, not the thread.

That is what the schema's optional `request_id` field is for. The caller passes it explicitly through context to each library call, and the library emits it on every record participating in that logical operation. A downstream consumer then filters on `request_id=<value>` to see the full trace, regardless of which threads were involved.

```cpp
const std::array<log_field, 6> fields{{
    {"event",        "tpm.key_load"},
    {"component",    "tpm2_esys"},
    {"outcome",      "success"},
    {"key_kind",     "transient"},
    {"request_id",   ctx.request_id},   // caller-supplied, threaded through
    {"source",       "esys_key_provider::load"},
}};
```

Do **not** synthesise a `request_id` from a thread-local global, an `std::this_thread::get_id()`, or a process-wide counter:

- Thread-local globals defeat per-test isolation, accumulate across long-lived threads, and force every consumer onto the same propagation model the library happened to pick (cross-reference `concurrency.md` Thread-local state).
- `std::this_thread::get_id()` is the OS thread ID by another name — it does not survive a thread hop.
- A process-wide counter is non-deterministic in tests and reveals call volume.

The caller owns `request_id` generation and propagation; the library only forwards what it is given.

## Implementing a logger adapter

### The contract

The `logger` port is defined in `logging.md`. The adapter must:

1. **Be thread-safe.** Multiple threads may call `log()` concurrently. Adapters wrapping a non-thread-safe sink serialize internally.
2. **Be `noexcept`.** Any I/O failure is swallowed — never thrown, never returned. A failed log is not an operation failure.
3. **Never retain pointers** across the call. The `std::string_view` keys/values and the field span are valid only for the duration of `log()`. Copy what you need to persist.
4. **Apply runtime filtering.** Consult the adapter's active level; drop records below it.
5. **Translate structured fields** to whatever format the underlying sink uses (`key=value` pairs, JSON, MDC, etc.). **Pick one format project-wide** — do not let two adapters render fields differently.

### Encoding and escaping field values

Adapters MUST escape characters that would otherwise allow a value to break out of its field and forge subsequent fields or log lines. **Log injection is a real threat class:** even though tpmkit does not log caller secrets directly, error codes, reason tokens, and `request_id` values can carry attacker-influenced bytes. A value containing `\n event=fake.injected outcome=success` would, in a naive `key=value` adapter, produce a fully forged log line that downstream consumers parse as legitimate.

Rules per format:

- **`key=value` format:** escape newlines (`\n` → `\\n`), carriage returns (`\r` → `\\r`), the delimiter character (`=` → `\\=`), the field separator (whitespace either escaped or contained by quoting the value), and any byte below `0x20` except tab. Adapters that wrap values in quotes must escape quote and backslash within.
- **JSON format:** use a real JSON encoder, never string concatenation. The encoder handles every escape automatically. Hand-rolled JSON in adapter code is forbidden — it is the single most common source of log-injection bugs in adapters of this kind.

The escaping is the **adapter's** responsibility, not the call site's. Library code passes raw `std::string_view` values; the adapter encodes on rendering. Two reasons: the call site does not know which format the adapter will use, and pre-escaping at the call site doubles the escaping when the adapter does it again.

Adapter contract test: a recording adapter under `tests/contract/` feeds adversarial values (newlines, quotes, control characters, format delimiters) into every adapter and asserts the rendered output round-trips cleanly through the format's parser. Cross-reference `tpm-write-tests` Contract tests.

### spdlog adapter — sketch, level mapping, sinks

The default adapter is spdlog. Read `references/spdlog-adapter.md` when implementing or modifying that adapter, or when deciding which sink to wire at the composition root. The reference covers:

- The `spdlog_logger` class sketch (`to_spdlog_level`, `key=value` rendering, the `noexcept` boundary).
- The level-mapping decision: `spdlog::level::critical` and `spdlog::level::off` are deliberately unmapped, and tpmkit `error` always maps to `spdlog::level::err`.
- Sink families (console, file, syslog, debugger output) and the tpmkit-relevant concern per family — including file permissions ownership, multi-process writers, and syslog truncation.
- Sink configuration tradeoffs: sync vs. async by event category, overflow policy on async sinks, and flush-on-shutdown.

Read it when working on the spdlog adapter; do not duplicate its content here.

### Recording adapter for tests

The test recording adapter (`src/adapters/mock/recording_logger`) captures records into a `std::vector<recorded_log>` for assertions in tests. Use it to verify the right *event name* and *level* fire at lifecycle boundaries, that required fields are present per the schema, and that no field value matches a known-secret pattern (the secret-leak sweep test, cross-reference `tpm-write-tests`).

Worked example using GoogleTest:

```cpp
TEST(esys_session_provider, logs_session_open_on_success) {
    recording_logger log;
    auto deps = make_test_deps();
    esys_session_provider provider{deps, log};

    const auto outcome = provider.open(session_kind::hmac);
    ASSERT_TRUE(outcome.has_value());

    ASSERT_EQ(log.records().size(), 1u);
    const auto& r = log.records().front();
    EXPECT_EQ(r.level,                 ::tpmkit::log_level::info);
    EXPECT_EQ(r.field("event"),        "tpm.session_open");
    EXPECT_EQ(r.field("component"),    "tpm2_esys");
    EXPECT_EQ(r.field("outcome"),      "success");
    EXPECT_EQ(r.field("session_kind"), "hmac");
    EXPECT_EQ(r.field("source"),       "esys_session_provider::open");
}
```

Notes on the pattern:

- **Assert on level + event name + field keys**, not on message text. Text is for humans and drifts; the schema is the contract (cross-reference `logging.md` Testing).
- **Strict-by-default**: an unexpected log call fails the test. The recording adapter's `records().size()` is the gate; do not silently accept extras.
- **The secret-leak sweep test** is one large parameterized test that drives every adapter through a controlled scenario emitting every event, then scans recorded values for known-secret patterns and fails if any appear. Lives under `tests/contract/`.
- **Do not assert on message text.** Wording drifts; the schema is the contract.

## Evolving the event schema

The schema lives in `references/event-schema.md`. It is part of the project's public surface — log consumers write queries against event names and field keys.

Stability contract:

- **Add a new event** or **add a new optional field** — minor version bump.
- **Add a required field to an existing event** — major version bump (existing consumers may have parsers that fail on unexpected fields, or queries that filter on the absence of a field).
- **Rename or remove an event or field** — major version bump. Prefer adding the new name and deprecating the old in parallel for a minor cycle, then removing in the major.

Process for adding a new event:

1. Open `references/event-schema.md` and find the right category (or propose a new category — see *Reserved categories* in that file).
2. Add the event with its level, required fields, and optional fields.
3. Add the call site in the library.
4. Add or update the recording-adapter test that covers the lifecycle boundary.
5. Update CHANGELOG (cross-reference `tpm-write-docs` Keep a Changelog format) — `### Added` for new event, `### Changed` for required-field additions.

Reject these in review:

- Ad-hoc events not in the schema. The schema is the source of truth; anything missing is added there first.
- Events whose name or fields collide with an existing event under a different concept ("`tpm.session_open` already means HMAC sessions; let's reuse it for policy sessions" — no, give policy sessions their own event).
- Optional fields that look like they belong in the message text. If it is data, it is a field.

## When to log vs. when not to

A log line should answer a question a future operator or auditor will actually ask. Lines that exist "just in case" accumulate noise that hides the lines that matter.

Log:
- The boundary between this library and a third-party (TSS2, OpenSSL, OS) on failure.
- Lifecycle events (session/key/handle creation and destruction).
- Backend selection at composition.
- Fallback selection when one happens.
- Configuration applied at startup.

Do not log:
- Per-call entry/exit at function granularity (use a profiler).
- Hot-loop bodies (cross-reference `performance.md` Hot-path discipline, `logging.md` Hot-path discipline).
- Anything on the never-log list (cross-reference `security.md` Logging).
- "About to do X" followed by "Did X" — pick one (the failure path can log the boundary error).

### What gets logged on error

Errors are logged **once, at the adapter boundary**, then propagate up as a domain `error` (or thrown `tpmkit_error`) with no further automatic logging at any layer above.

- **Backend errors** (`TSS2_RC`, OpenSSL, `errno`) — the adapter logs at `error` *before* translating. The original third-party numeric code (`error_code=...`) and sanitized decoded backend diagnostic text (`backend_error_description=...`) live in this single record when available; nothing above the adapter sees them. Cross-reference `error-handling.md` Translating third-party errors.
- **Security failures** — logged with the `security_failure` category, deliberately coarse: no precise `source` field, no caller-visible detail beyond the category, no secret-derived bytes. Cross-reference `error-handling.md` Security-sensitive failures and `references/event-schema.md` Forbidden combinations.
- **Input and resource errors** — logged once at the boundary that detected them (public-API validation, or the adapter that produced the resource failure), not at every level the error propagates through.

The library does **not** auto-log:

- An `outcome<T, error>::failure(err)` return.
- A thrown `tpmkit_error`.
- The domain `error.message` string at any layer above the boundary.

Consequence: an `error`-level record exists for every third-party-boundary failure, exactly once. There is no record stating "function X returned error Y to caller Z" — that is the consumer's app-level concern. If the consumer catches a `tpmkit_error` and silently retries or papers over it, nothing about that retry appears in tpmkit's logs beyond the original boundary record.

## Out of scope

The library deliberately does not implement these. Consumers that need them wire them at the adapter or sink layer.

- **Distributed tracing / OpenTelemetry context propagation.** The library accepts an opaque `request_id` and emits it on every record; how that ID is generated, propagated across services, and ingested into a tracing system is the consumer's concern. The library never imports an OTel SDK or similar tracing framework.
- **Audit-grade guarantees** — signed log lines, append-only sinks, tamper-evident chaining, write-on-confirm semantics. These are sink-level features. A consumer requiring them wires a sink that provides them; the library's contract is "emit the record correctly," not "guarantee its persistence under attack."
- **Log rotation, retention, archival, encryption-at-rest.** Sink-level concerns. The spdlog rotating-file sink, syslog, journald, and centralized aggregators each provide their own policies; the library does not.
- **PII redaction at the sink.** The library guarantees secrets never appear in records (the never-log rules); consumer-defined PII redaction (e.g., for `request_id` values that look like email addresses) is downstream of the library.
- **Centralized log aggregation, ingestion-pipeline schemas.** The library emits records in a stable schema (`references/event-schema.md`); how those records get to Elastic, Loki, Splunk, or CloudWatch is downstream.

If a use case appears to require library-level support for one of the above, raise an ADR (cross-reference `tpm-write-docs` ADR format) before adding code — it is more often a misshapen requirement than a real gap.

## Common mistakes

- **Inventing an event name at the call site instead of consulting the schema.** Two adapters end up with `tpm.session_started` and `tpm.session_open` for the same thing; consumers' queries miss half the lifetime.
- **Splitting an event's schema name and message into parallel constants.** The call site
  can then pair `message=A` with `event=B`. Keep them in one descriptor and pass that
  descriptor through helper APIs.
- **Stuffing variable data into the message text.** Use a field. The message is for humans; fields are for machines.
- **Logging the raw `TSS2_RC` alongside a translated domain error in the same record.** The numeric code goes at the adapter boundary; the domain error goes at the operation result. Mixing them couples consumers to the third-party numbering scheme.
- **Copying decoded backend diagnostic text into public `error.message`.** Backend text is dependency-owned and may expose implementation details. Keep it in `backend_error_description` at the adapter boundary.
- **Forgetting the `outcome` field** on a terminal log line. Without it, success and failure records look identical to a query.
- **Logging the TPM handle value** as a field. Handle values are sensitive (they can leak ASLR offsets, persistent-handle ownership, or session-state correlation). Use `handle_kind` (`transient` / `persistent` / `session`) instead.
- **Skipping `noexcept` on the adapter override.** Every override of `logger::log` is `noexcept` per the port contract; an exception escaping is a contract violation.
- **Letting the recording adapter assert on message text.** The text is for humans and will drift; assert on level + event + fields.
- **Choosing two field-rendering formats** ("this adapter does JSON, that one does `key=value`"). Pick one project-wide. Inconsistency makes log search useless across adapter boundaries.
- **Putting a precise `source` value on a `security_failure` record.** That rebuilds the very oracle the rules in `error-handling.md` (Security-sensitive failures) and `security.md` (Failure handling) deliberately block — distinguishing `mac_validator::verify` from `padding_validator::verify` in the log lets a log-stream reader tell which check failed even when the caller-visible outcome is just `security_failure`. Either omit `source` on those records or use a deliberately coarse common value (`source=verifier::check`). See `references/event-schema.md` Forbidden combinations.
- **Using `file:line` as `source`.** The schema requires `class::method` form. File paths leak structure that class names do not, churn faster across refactors, and hint at line numbers that make every patch a potential schema break.
- **Reaching for a singleton logger** (`logger::instance()`, `spdlog::get(...)` from inside a class) instead of taking `logger&` in the constructor. Tests cannot isolate from each other; two `tpmkit` instances in the same process cannot use different loggers. See *Owning the logger* above and `tpm-write-code` Anti-patterns.
- **Mapping tpmkit `error` to spdlog `critical`** (or any other backend's `fatal`/`emerg`/`alert` slot) in an adapter to make library errors "stand out." That slot is reserved for consumer application code; tpmkit `error` always maps to the backend's plain `error` level. See `references/spdlog-adapter.md` (level mapping).
- **World-readable log files.** A file sink at default mode (often `0644`) leaks operational metadata — key-handle loads, session opens, persistent-handle evictions — to any local user, widening the threat model beyond what `security.md` assumes. The records are clean of secrets but not of operational signal. **The consuming application owns this** — tpmkit and the adapter cannot enforce it portably. Set `umask(0077)` before constructing the sink, or use a `0700` parent directory, or harden the systemd unit (`UMask=0077`). See `references/spdlog-adapter.md` (sink families).

## Cross-references

- `logging.md` — port shape, levels, never-log rules, filtering, hot-path discipline.
- `security.md` Logging — never-log content list (authoritative).
- `error-handling.md` — adapter-boundary translation; what to log before translating.
- `concurrency.md` — adapter thread-safety, no unbounded async queues.
- `performance.md` — hot-path discipline; compile-time elision via `TPMKIT_LOG_MAX_LEVEL`.
- `library-api-design.md` — versioning rules that apply to the schema as public surface.
- `tpm-build-config` — vcpkg pin for spdlog, async-sink build options.
- `tpm-write-code` — Null Object pattern (used by `noop_logger`), Anti-patterns (singleton/global injection bans that apply equally to the logger), and Dependency inversion guidance.
- `tpm-write-tests` — recording-adapter test patterns; the secret-leak sweep test.
- `tpm-debug` Diagnosis vs. reproduction — why logs narrow the search but do not reproduce bugs in this library, and which tools (property tests, fuzz corpus, swtpm snapshots, sanitizers, customer reproducer) do.
- `references/event-schema.md` — the standard event and field schema.
- `references/owning-the-logger.md` — constructor-injection pattern with no-op default, composition-root wiring, anti-patterns.
- `references/spdlog-adapter.md` — the spdlog adapter sketch, level mapping, sink families, and sink-choice tradeoffs.

## Error Handling

* **No schema entry matches the event being logged.** Stop. Do not invent a name at the call site. Open `references/event-schema.md` and add the new event under the right category (or propose a new category) before writing the call site. Update the CHANGELOG (`### Added`) per `tpm-write-docs`.
* **`outcome=failure` record carries `source` with a precise `class::method` and `error_category=security_failure`.** Forbidden combination — rebuilds the oracle the rules block. Either omit `source` or replace with the deliberately coarse common value documented in `references/event-schema.md` Forbidden combinations.
* **Adapter override of `logger::log` is not `noexcept`.** Contract violation. Add `noexcept` and route every internal failure path to a swallow (no rethrow, no return-via-exception).
* **Field value would be high-cardinality or attacker-influenced raw bytes.** Replace with a bounded category (`size_bucket=medium` instead of `size=1024`; `handle_kind=transient` instead of the raw handle value). When in doubt, omit the field — the never-log rules in `security.md` are the backstop, not the first line of defence.
* **Schema change is a rename, removal, or required-field addition.** Major version bump per the stability contract above. Prefer adding the new name and deprecating the old in parallel for a minor cycle, then removing in the major.
* **Recording-adapter test asserts on message text.** Wording drifts; the schema is the contract. Rewrite the assertion to check level + event name + field keys instead.
* **Hot-path call site goes through `logger_.log(...)` directly.** Compile-time elision is lost. Switch to the `TPMKIT_LOG_TRACE_` / `TPMKIT_LOG_DEBUG_` macros so the field-array construction is guarded by `TPMKIT_LOG_MAX_LEVEL`.
* **Logger ownership is unclear (which class owns the lifetime?).** Re-read `references/owning-the-logger.md` Composition-root wiring. The composition root owns the logger; every adapter takes a reference. If a non-composition site wants to own a logger, that is the bug — fix the wiring, not the lifetime.
