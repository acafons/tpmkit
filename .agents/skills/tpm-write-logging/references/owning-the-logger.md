# Owning the logger

The `logger` is a port like `key_provider` or `crypto_primitives`, but it is a **cross-cutting concern** — the operation's outcome does not depend on whether logging is enabled. That justifies a slightly different ergonomic treatment from functional ports: constructor injection with a **stateless no-op default**. Singletons, thread-locals, and globals are forbidden by the existing rules and not used here.

## Where logging actually lives

Logging concentrates at the boundary, not across the whole codebase:

- **Adapters log.** They translate third-party errors, emit lifecycle events, and select fallbacks. This is where the real logging happens.
- **The composition root logs.** Startup events: backend selection, configuration applied, startup failure. One place per binary.
- **Domain code mostly does not log.** Value objects, entities, and simple domain services do not have anything interesting to say at the logging layer; the adapter on the other side of the call already logs the relevant boundary event.

In a tpmkit-sized library this lands at roughly five to ten classes that take a logger, all of which are adapters or composition wiring. It is not "every class."

## The pattern

Adapters and the composition wiring take `logger& log` in the constructor, with a default of `noop_logger::instance()` so callers that do not care can omit it.

```cpp
class esys_session_provider final : public session_provider {
public:
    explicit esys_session_provider(deps_t deps,
                                   logger& log = noop_logger::instance());

    // ...

private:
    logger& log_;
};
```

`noop_logger::instance()` returns a reference to a function-local static of a stateless `noop_logger` type — its `log()` body is empty and `noexcept`. C++11 guarantees the static is thread-safely initialized; there is no race, no per-test contamination, no hidden mutable state. This is the **Null Object** pattern that `tpm-write-code` already endorses, applied to the logger port.

The pattern delivers, with no extra wiring:

- **Production:** the composition root passes the real spdlog adapter; every class logs.
- **Tests that assert on logging:** pass a `recording_logger`; the test owns the recording adapter for its scope.
- **Tests that do not care about logging:** omit the argument entirely; the class compiles, runs, and stays silent.
- **The library-with-logging-disabled use case** that `logging.md` mandates: identical to the silent-test case.

## Why this is not the singleton anti-pattern

The singleton anti-pattern is about *mutable, configurable, process-wide state hidden from constructors* — every consumer implicitly couples to whatever the singleton was last configured with, and tests cannot isolate from each other. A stateless no-op is none of those things:

| Mutable singleton (forbidden) | Stateless no-op default (acceptable) |
|---|---|
| `logger::instance()` returns a configurable, mutable global. | `noop_logger::instance()` returns a reference to an empty function-local static. |
| Tests must reset state between runs. | Stateless: nothing to reset. |
| Configuration changes propagate invisibly to every caller. | No configuration. The no-op never does anything. |
| Cannot run two instances with different logging behaviour in the same process. | Default is just a default; each adapter is constructed with whatever logger the caller picks. |

The stateless-no-op default has the same shape as `std::cout` — a reference to a fixed, stateless thing — which is not what the singleton ban is aimed at.

## Composition-root wiring

The composition root constructs the logger first and destroys it last, so every adapter's `logger&` reference is valid for the adapter's entire lifetime. Lifetime ordering is the only correctness rule; everything else falls out.

```cpp
namespace tpmkit::composition {

std::unique_ptr<key_provider> make_default_key_provider(const config& cfg) {
    // 1. Build the logger first; it must outlive everything that holds a reference.
    static auto sink = ::spdlog::stdout_color_mt("tpmkit");
    static adapters::spdlog_adapter::spdlog_logger app_logger{sink};

    // 2. Build dependencies that take the logger by reference.
    auto session_provider =
        std::make_unique<adapters::tpm2_esys::esys_session_provider>(
            /* deps */, app_logger);

    auto key_provider =
        std::make_unique<adapters::tpm2_esys::esys_key_provider>(
            /* deps */ std::move(session_provider), app_logger);

    return key_provider;
}

} // namespace tpmkit::composition
```

Notes on the composition root:

- **Logger first, consumers second.** Every adapter holds `logger&`, so the logger must outlive every adapter that references it. The simplest realisation is a static (as above) or a member of the composition object that owns the adapters; either way the lifetime ordering is explicit, not accidental.
- **Pass by reference, not by `shared_ptr`.** No copying, no atomic refcount. Use `std::shared_ptr<logger>` only when ownership is genuinely shared across contexts that have no common owner.
- **One logger per composition.** The library does not support per-class loggers. Pick one logger at composition and route everything through it; filtering by `component` or `event` is the recording adapter's or sink's job, not the library's.
- **Logging from destructors and cleanup paths is allowed and often necessary** — RAII close paths emit `tpm.session_close` and `tpm.key_evict` events, and these are the audit trail for resource teardown. Two correctness conditions: (1) the logger reference must still be valid, which the lifetime-ordering rule above guarantees as long as composition tears down in reverse construction order; and (2) the adapter's `log()` is `noexcept` per the port contract, so destructor logging cannot accidentally throw. Do not log from `std::terminate` handlers or async-signal handlers — that is a separate constraint owned by `concurrency.md` (signal-safety) and the never-log rules.

## Anti-patterns

- **Singleton logger access** (`logger::instance()` returning a configurable mutable global, `spdlog::get("name")` from inside a class). Forbidden by `tpm-write-code` Anti-patterns and `architecture.md` Dependency inversion. Tests cannot isolate from each other; two `tpmkit` instances in the same process cannot use different loggers.
- **Thread-local logger** (`thread_local logger* current_logger`). Cross-reference `concurrency.md` Thread-local state. Per-thread loggers would require thread-local storage owned by the library, which the rules forbid.
- **Setter injection** (`adapter.set_logger(&log)` after construction). Two-phase initialization is forbidden by `code-standards.md` (Avoid two-phase initialization). The constructor either receives the logger or defaults to no-op; there is no third state.
- **Optional `logger*` (raw pointer) parameter that may be null.** Reintroduces null checks at every call site. Use `logger&` with a no-op default instead.
- **Per-method logger parameter** (every method takes `logger&` as an argument). Adds permanent API noise for a cross-cutting concern. The constructor parameter pattern keeps the API clean.
