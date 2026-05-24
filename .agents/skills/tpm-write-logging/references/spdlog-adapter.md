# spdlog Adapter Reference — tpmkit

This file is the implementation reference for the project's default logger adapter (spdlog). It covers the adapter sketch, the level mapping, the sink families, and the sink-configuration tradeoffs (sync vs. async, overflow policy, flush on shutdown). Read this when implementing or modifying the spdlog adapter, or when wiring a sink at the composition root.

The general adapter contract — thread-safety, `noexcept`, no pointer retention, runtime filtering, encoding and escaping rules — lives in `SKILL.md` (Implementing a logger adapter). Read that first.

## How file logging works end-to-end

Use this section to build the mental model of the pipeline before diving into the adapter sketch or sink details. The rules — file permissions, multi-process writers, sync vs. async, overflow policy, flush — live in *Sink families* and *Sink choice* below; this section is just the data flow and lifecycle.

### The pipeline

```
domain/adapter code
     │ logger_.log(level, message, fields)
     ▼
tpmkit::logger  (port — abstract base class)
     │ vtable dispatch
     ▼
spdlog_logger  (the adapter, src/adapters/logging/spdlog/)
     │ renders field span → "msg key=value key=value..."
     │ maps tpmkit::log_level → spdlog::level
     │ calls sink_->log(...)
     ▼
::spdlog::logger  (third-party object built at the composition root)
     │ runtime level filter
     │ pattern formatter (timestamp, logger name, level prefix)
     │ fans out to every registered sink
     ▼
::spdlog::sinks::basic_file_sink_mt  (or rotating_/daily_)
     │ holds open file descriptor + std::mutex (in-process)
     │ takes mutex → fwrite → optional flush → releases
     ▼
write() syscall → kernel page cache → disk
```

Five stages, each owned by a different layer:

1. **Library call site.** Hands a structured record to a `logger&`. No file knowledge — could be no-op, console, syslog, or file underneath.
2. **`spdlog_logger` adapter.** Translates the level, renders the field span into one `key=value` string, calls into the spdlog logger. Still no file.
3. **`::spdlog::logger`.** Applies its own runtime level filter, prefixes timestamp/name/level via its `pattern`, fans the formatted line out to each attached sink. One spdlog logger can hold multiple sinks (`dist_sink` or a multi-sink logger).
4. **The sink.** Where the file lives. Opens the file in its constructor (`fopen` on POSIX; mode is `0644` masked by process umask). Holds a `std::mutex` that serializes concurrent writes within one process. Per `log()` call: takes the mutex, writes the formatted bytes, optionally flushes, releases. Tracks size (rotating) or date (daily) privately and `rename()`s + `fopen()`s a fresh file at threshold. Closes in its destructor.
5. **Kernel.** `write()` deposits bytes into the page cache; flushed to disk by writeback. spdlog does not `fsync` per record — durability requires an explicit `flush()` and whatever sync the sink configuration applies on top.

### File lifecycle in one process run

1. Composition root constructs the spdlog sink → **file opened** (the moment umask is locked in).
2. Composition root constructs `spdlog_logger` wrapping the spdlog logger → handed to every adapter as `logger&`.
3. Library runs; each `log()` call writes one line.
4. On shutdown, composition root calls `flush()` → drains the async queue (if any) and flushes the sink.
5. Composition object teardown destroys the spdlog sink → **file closed.**

The mental shift: **the library never sees a file.** It writes to a port. The composition root chooses the port's implementation (spdlog), the spdlog logger's sinks (`basic_file_sink`, etc.), and the file path/mode/rotation policy. tpmkit's contract is "emit the record correctly"; everything from formatted-line-to-disk is the consumer's wiring.

## spdlog adapter sketch

```cpp
namespace tpmkit::adapters::spdlog_adapter {

class spdlog_logger final : public ::tpmkit::logger {
public:
    explicit spdlog_logger(std::shared_ptr<::spdlog::logger> sink)
        : sink_(std::move(sink)) {}

    void log(::tpmkit::log_level level,
             std::string_view message,
             gsl::span<const ::tpmkit::log_field> fields) noexcept override {
        try {
            const auto sl = to_spdlog_level(level);
            if (!sink_->should_log(sl)) return;

            ::fmt::memory_buffer buf;
            ::fmt::format_to(std::back_inserter(buf), "{}", message);
            for (const auto& f : fields) {
                ::fmt::format_to(std::back_inserter(buf),
                                 " {}={}", f.key, f.value);
            }
            sink_->log(sl, std::string_view(buf.data(), buf.size()));
        } catch (...) {
            // Logger is no-throw by contract.
        }
    }

private:
    static ::spdlog::level::level_enum
    to_spdlog_level(::tpmkit::log_level l) noexcept {
        switch (l) {
            case ::tpmkit::log_level::trace: return ::spdlog::level::trace;
            case ::tpmkit::log_level::debug: return ::spdlog::level::debug;
            case ::tpmkit::log_level::info:  return ::spdlog::level::info;
            case ::tpmkit::log_level::warn:  return ::spdlog::level::warn;
            case ::tpmkit::log_level::error: return ::spdlog::level::err;
        }
        return ::spdlog::level::info;
    }

    std::shared_ptr<::spdlog::logger> sink_;
};

} // namespace tpmkit::adapters::spdlog_adapter
```

Notes on the sketch:

- `fmt::*` here is **fmtlib** (the third-party library), not `std::format` (which is C++20 and not available on this project's C++17 baseline). spdlog already depends on fmtlib internally — using `fmt::memory_buffer` and `fmt::format_to` in the adapter therefore costs no extra dependency. Do not replace these with `std::format`/`std::format_to` until the project moves to C++20.
- The `try`/`catch (...)` is at the adapter boundary specifically because the contract is `noexcept`. Cross-reference `error-handling.md` No silent failure: thread entry points and adapter `noexcept` boundaries are the two places `catch (...)` is allowed.
- The format above is `key=value` separated by spaces — pick once for the project; do not let two adapters render fields differently. JSON output is also fine if that is what the project standardizes on; whichever is chosen, it lives in this skill, not in two adapter-specific decisions.
- For high-throughput cases use `spdlog::async_logger` with a **bounded** queue. Document the queue depth; an unbounded queue is forbidden (cross-reference `concurrency.md` Forbidden patterns).
- The adapter does not add timestamps, hostnames, PIDs, or correlation IDs — spdlog (or its sink) adds those. The library never emits them (cross-reference `logging.md` Structured fields).

## spdlog level mapping

spdlog ships two extra level values that `to_spdlog_level` deliberately ignores:

- **`spdlog::level::critical`** has no counterpart in `tpmkit::log_level`. The library has no `fatal`/`critical` level by design (cross-reference `logging.md` Levels: *"There is no `fatal` level"*). Process-abort decisions belong to the consumer; conditions severe enough to abort still flow through `error` paired with a returned/thrown failure. Adapters MUST NOT promote a tpmkit `error` record to spdlog `critical` to make it "stand out" — the slot is reserved for consumer application code that wants its own top tier above the library's records. tpmkit `error` always maps to `spdlog::level::err`.
- **`spdlog::level::off`** is a filter sentinel, not a level you log at. Its tpmkit equivalents are wiring the `noop_logger` (runtime "off") and the `TPMKIT_LOG_MAX_LEVEL` compile-time ceiling (build-time elision). The library cannot emit a record at `off` because `log_level` has no such value, and a `log()` call with no meaningful level cannot be expressed.

The same logic applies to any other logger backend that defines extra level slots (e.g., glog's `FATAL`, syslog's `LOG_EMERG`/`LOG_ALERT`/`LOG_CRIT`): map only what `tpmkit::log_level` defines, and leave the higher slots free for the consumer's own use.

## Sink families: console, file, syslog, debugger output

The library does not pick a sink; the consumer does, at the composition root. Each family interacts with tpmkit's contracts differently — pick by tradeoff, not by familiarity. What follows is the tpmkit-relevant concern per family, not a substitute for spdlog's documentation.

**Console** (`spdlog::stdout_color_mt`, `spdlog::stderr_color_mt`). Fit for development, simple CLI tools, and containerized binaries where stdout/stderr is captured by the runtime. Use `stderr` for tpmkit records — `stdout` is reserved for the consumer's own program output, and mixing the two breaks any downstream that pipes stdout. Synchronous; `flush()` is a no-op.

**File** (`basic_file_sink`, `rotating_file_sink`, `daily_file_sink`). Production default. Two non-obvious concerns:

- **File permissions.** tpmkit keeps secrets out of records (cross-reference `security.md` Logging), but lifecycle data — which key handle loaded, when an HMAC session opened, when a persistent handle was evicted — is operational metadata about the host. World-readable logs widen the threat model beyond what the rules assume. **Permissions are the consuming application's responsibility, not the library's or the adapter's:** tpmkit never opens a file, and spdlog's `basic_file_sink` opens with `0644` (masked by umask) and exposes no mode-at-open hook — an after-the-fact `chmod` from the adapter has a race window where the file is briefly world-readable. The composition root sets the mode through one of: `umask(0077)` immediately before constructing the file sink (so spdlog's `open(O_CREAT, 0666)` is masked to `0600`), a restrictive parent directory (`0700`, daemon-owned) so traversal is blocked even if the file ends up `0644`, or systemd unit hardening (`User=`, `UMask=0077`, `ReadWritePaths=`) for daemon consumers. Rotation itself is downstream policy (cross-reference `SKILL.md` Out of scope); pick a rotation that does not truncate an in-flight record (size-based with line-boundary cuts, or daily).
- **Multi-process writers.** spdlog's file sinks do not coordinate across processes — each process owns a private handle and a private size counter. Three failure modes: writes longer than `PIPE_BUF` (~4096 B) can interleave between processes; a `rotating_file_sink` shared across processes hits a **silent rotation race** where one process renames the file and another keeps writing to the now-renamed inode (records land in the wrong rotated file with no error); and external rotation (logrotate) leaves spdlog writing to an unlinked inode because there is no SIGHUP-reopen. The recommendation is **one file per process** (`app-${pid}.log`, `app-${role}.log`) — aggregation downstream via `cat *.log | sort -k1`, rsyslog, fluentd, or vector. If multiple processes must share a file, route through syslog/journald (daemon serializes by design — see the **Syslog** entry below) rather than `flock`-per-write, which contends on every record.

**Syslog** (`syslog_sink`). Useful when the consumer is a daemon under systemd/rsyslog and wants centralized retention with no in-process file handling. Two caveats specific to tpmkit:

- **Message truncation.** Most syslog implementations truncate at ~1024 bytes per record. A truncation mid-JSON produces parseable-but-wrong output downstream — fields after the cut vanish silently, which is exactly the failure mode that defeats `request_id`-based correlation. If records routinely carry many fields, prefer journald (handles structured fields natively as separate journal fields) or a file sink.
- **Priority mapping.** Map tpmkit levels straight across: `error`→`LOG_ERR`, `warn`→`LOG_WARNING`, `info`→`LOG_INFO`, `debug`→`LOG_DEBUG`, `trace`→`LOG_DEBUG`. Never use `LOG_EMERG`/`LOG_ALERT`/`LOG_CRIT` — same rule as spdlog `critical` (see *spdlog level mapping* above): those slots are reserved for the consumer's own application code.

**Debugger output** (`msvc_sink` on Windows = `OutputDebugString`; `os_log_sink` on macOS = `os_log`). Visible only when a debugger is attached on Windows, or via `Console.app`/`log stream` on macOS. Wire as a **secondary** sink during development alongside a primary file or syslog sink so records do not vanish when no one is watching. Never a primary sink in production. Always synchronous; the sync/async tradeoffs in the next section do not apply.

**Multiple sinks.** spdlog composes sinks via `dist_sink` or a multi-sink logger; the per-sink level filter lets `debug` land in a file while `info`+ goes to syslog. The tpmkit adapter does not need to know — composition lives in the `::spdlog::logger` passed into `spdlog_logger`'s constructor.

## Sink choice: sync, async, overflow, and flush

The adapter's underlying sink configuration is where data-loss versus latency tradeoffs land. Three decisions, each with a security-relevant default:

**Sync vs. async sink.** Async sinks (background thread, queue between caller and writer) reduce caller latency but **lose pending records on crash**. Some categories of event must be sync to survive a crash:

| Category | Sink |
|---|---|
| `composition.*` startup and teardown events | Sync. The information is needed to diagnose start-up failures, exactly the kind of event that runs immediately before a crash. |
| `*.security_failure` records, persistent-handle define/evict, key-load events for persistent keys | Sync. Audit-relevant lifecycle that loses meaning if it disappears at crash time. |
| Adapter-boundary `*.backend_error` records | Sync recommended. The error is by definition a failure mode that may precede a crash. |
| `info`, `debug`, `trace` records on hot paths | Async permitted. Volume justifies the latency win; loss on crash is acceptable for this tier. |

A single adapter wrapping spdlog can route different events to different sinks via the sink's level filter or a custom routing layer. Default to a single **sync** sink unless measured throughput requires async — premature async is a common mistake that turns into "where did the last 200 records before the crash go?" the first time something fails badly.

**Overflow policy on async sinks.** When the bounded queue fills, choose the policy explicitly:

| Policy | Behavior | When to use |
|---|---|---|
| `block` | Caller waits for queue space. | Audit-critical events you cannot afford to lose. Risk: caller stalls, possible cascading slowdown. |
| `discard_new` | New records dropped silently. | Last resort. **Do not use for security or lifecycle events** — silent loss of the very record you wanted to keep. |
| `discard_oldest` | Oldest queued records evicted. | Acceptable for `debug`/`trace` traffic where recency matters more than completeness. |

`spdlog::async_logger` exposes these via `spdlog::async_overflow_policy`. **Unbounded queues are forbidden** (cross-reference `concurrency.md` Forbidden patterns) — a backed-up queue is a memory leak with extra steps.

**Flush on shutdown.** Async sinks have pending records at process exit; the composition root must flush before destruction:

```cpp
~composition() {
    if (logger_) {
        logger_->flush();   // adapter exposes flush; spdlog::logger has it directly
    }
}
```

Without the flush, the last events before clean shutdown — including any `composition.shutdown` event — are silently lost. For sync sinks `flush()` is a no-op; for async sinks it is the difference between "we have a record" and "we lost the diagnostic for this run." Adapters expose a `flush()` method on the `logger` port for this purpose; the no-op adapter's `flush()` is empty.
