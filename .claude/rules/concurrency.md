# C++17 Concurrency Rules

These rules build on `code-standards.md` (resource handling, `noexcept`) and `library-api-design.md` (thread-safety contracts). They define how the library implements concurrent code, not how it documents it.

## Default stance

- Default to **single-threaded**. Add concurrency only when a measured requirement justifies it. Concurrency is not free — it adds correctness and review cost on every line that touches shared state.
- Default thread-safety contract for new types is **thread-compatible**: independent instances are safe to use from different threads, but a shared instance is not. Promote to thread-safe only when a use case demands it, and document the contract per `library-api-design.md`.
- Free functions on the public API are thread-safe by default. Internal free functions follow the same rule unless they capture mutable static state — in which case they are not free functions in spirit and need a different home.

## Synchronization primitives

- `std::mutex` for mutual exclusion. `std::shared_mutex` only when reads dominate writes by a measured margin and the critical section is non-trivial. The added cost of a shared mutex is rarely paid back by short read sections.
- Hold a lock through the smallest scope that preserves the invariant. Never call back into user-supplied callbacks, response factories, observer hooks, logger adapters, or custom deleters while holding a lock — copy the required state locally, release the lock, then call out.
- Use `std::lock_guard` for fixed-scope locking and `std::unique_lock` only when you need conditional unlock or hand-off (e.g., to `condition_variable::wait`). Do not use `std::scoped_lock` (C++17 multi-mutex) without a documented reason — it usually indicates a lock-ordering problem the design should solve instead.
- `std::atomic<T>` only for trivially copyable types where a single read-modify-write is the entire operation. The moment you need two atomics to be consistent with each other, you need a mutex, not two atomics.
- One-time initialization uses `std::call_once` with `std::once_flag`. Never roll a hand-written double-checked locking pattern — it is a known C++ memory-model trap.
- No spinlocks. The OS scheduler is better than user-space busy-waiting for any wait we are likely to encounter.

## Lock ordering and deadlock avoidance

- When a thread must hold two or more locks, declare a project-wide acquisition order and follow it everywhere. Lock-ordering inversions are the dominant source of deadlocks in this kind of code.
- Document the acquisition order at the type that owns the locks, not at every call site. If the order is non-obvious, the design is wrong.
- Never call into a port (a virtual method on an abstract base) while holding an internal lock. The adapter's behavior is outside our control and may take its own locks in any order.

## Threads and tasks

- `std::thread::detach` is forbidden. A detached thread cannot be joined, cannot be cancelled, and outlives the type it borrowed pointers from. Use a joinable thread, a worker pool, or `std::async` with a `std::future` whose destructor blocks.
- Every joinable thread must be joined before its owning object's destructor returns. Wrap the thread in an RAII helper that joins on destruction — the standard library does not provide this in C++17 (no `std::jthread`).
- `std::async(std::launch::async, ...)` is the only `std::async` form we use. Default-launch policy may run inline at the `.get()` call site, which silently kills the concurrency you asked for.
- Treat thread creation as expensive. Reuse a worker pool for work submitted in volume; create one-off threads for setup or shutdown work only.

## TPM-specific concurrency

- A TPM connection (FAPI context, ESYS context) is **not** thread-safe. Each context is owned by one thread at a time. Either confine the context to a single thread, or guard it with a mutex that serializes all calls. Document the choice on the adapter type.
- TPM sessions are stateful — nonces and HMAC chains are updated on every command. A session used concurrently corrupts in ways that surface as `security_failure` outcomes much later. Treat session ownership as a hard invariant, not a guideline.
- Adapters that hold a TPM context across multiple operations document whether the context is per-thread or shared-with-mutex. The composition root sees this contract and wires accordingly.

## OpenSSL threading

- Targeted OpenSSL versions are 1.1.0 and later, which initialize their own locking. Do not install the legacy `CRYPTO_set_locking_callback` / `CRYPTO_THREADID_set_callback` shims — they are removed and calling them is a build break on the supported set.
- An `EVP_*` handle is **not** thread-safe. Each handle is owned by one thread at a time. Allocate per-operation (cheap) rather than caching a handle for cross-thread reuse.
- The OpenSSL error queue is thread-local. Errors raised on one thread are invisible to another. Adapters that span threads must drain the queue on the same thread that produced the error.

## Thread-local state

- Thread-local statics are an anti-pattern in this codebase. They are invisible to dependency injection, defeat per-test isolation, and accumulate across long-lived threads. The exception is unavoidable third-party state (the OpenSSL error queue, errno) — never our own.
- Pass context (a session handle, a logger field set, a request ID) explicitly through function arguments or a context object. Implicit context is bug-friendly.

## Exceptions and threads

- An exception that escapes a `std::thread`'s entry function calls `std::terminate`. Wrap every thread entry in a `try`/`catch` that translates to a domain error or logs and exits cleanly. Cross-reference: `error-handling.md` "No silent failure" — `catch (...)` at thread entry points is one of the two places it is allowed.
- An exception inside a `std::async` task is captured into the future. The future's destructor will rethrow if `.get()` was never called — be sure every async task's future is consumed.
- Move operations and destructors are `noexcept` (per `code-standards.md`); this is doubly important for types used across threads, since a throwing move during `std::vector<T>` reallocation in a worker pool is unrecoverable.

## Forbidden patterns

- **Double-checked locking** without `std::call_once` or `std::atomic<bool>` with the right memory orderings. Use `std::call_once`.
- **Mutexes inside signal handlers.** Async-signal-safety is a separate discipline; keep crypto work out of signal handlers.
- **Sleeping in a loop to wait for a condition.** Use `std::condition_variable::wait_for` with a predicate.
- **Unbounded queues** between threads. A backed-up queue is a memory leak with extra steps. Use a bounded queue with backpressure or block the producer.
- **Reading and writing the same non-atomic variable from two threads** without synchronization, even when "the worst that can happen" looks benign. The C++ memory model makes this a data race, which is undefined behavior — the compiler is allowed to assume it cannot happen, and will optimize accordingly.

## Testing implications

- The full test suite must pass clean under **ThreadSanitizer**. A TSan failure is a release blocker. See `security.md` (Build hardening) for the sanitizer matrix.
- Concurrency tests must be deterministic. If a test relies on `std::this_thread::sleep_for` to "wait for the other thread to do its thing," the test is broken — synchronize explicitly with `std::condition_variable`, `std::promise`/`std::future`, or `std::latch` (C++20 — emulate with a counter and condition variable until then).
- Stress tests (many threads, many iterations) live in `tests/integration/` or a dedicated `tests/stress/` tier and run as part of nightly CI, not on every PR.
