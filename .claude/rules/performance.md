# C++17 Performance Rules

These rules govern performance work — when to do it, how to measure it, and what may be sacrificed for it. They build on `code-standards.md` (RAII, move semantics, `noexcept`) and `architecture.md` (polymorphism choices). Performance is a feature, not a license to break correctness, security, or readability.

## Default stance

- **Correctness, security, then performance.** A faster implementation that is harder to reason about is a regression. The crypto and TPM domain has a high audit cost — every line is read by reviewers and security analysts long after it is written.
- **Don't optimize without measuring.** Intuition about what is fast in modern C++ is unreliable. Measure before changing anything in the name of performance, and measure again to confirm the change paid off.
- **Default to the obvious implementation.** Reach for clever (template metaprogramming, hand-rolled SIMD, pointer arithmetic on hot paths) only when measurement justifies the loss of clarity.

## Measurement discipline

- Benchmarks live under `bench/` with a structure mirroring `tests/`. Use a single benchmarking framework project-wide (Google Benchmark is the default — pick once, document in `tpm-build-config`).
- Benchmark builds use **release flags only**: `-O3 -DNDEBUG`. A measurement taken under `-O0`, `-Og`, or any sanitizer is not a measurement — sanitizer-instrumented code can be 2–20× slower with non-uniform overhead.
- Pin the environment for any measurement worth recording: single CPU, fixed frequency governor, no other load. CI runners are noisy; benchmarks that gate releases run on a dedicated host.
- Report mean **and** variance. A 5% mean improvement with overlapping standard deviations is noise.
- Compare like for like. Comparing a release build against a debug build, or `march=native` against generic, produces meaningless deltas.
- `-march=native` is forbidden in shipped builds — it bakes the build host's CPU into the binary and breaks portable distribution. Use it only in benchmark-host configurations where the host is also the target.

## Performance budgets

- Each performance-sensitive public operation has a documented budget: latency at p50 and p99, plus throughput where it applies. Budgets live next to the benchmark, not in a wiki.
- A PR that regresses a budgeted operation by more than **10%** must include a measured rationale and reviewer sign-off. A PR that regresses it by more than **25%** is rejected unless it is fixing a correctness or security bug.
- A PR that *improves* a budget by more than 10% updates the budget. Stale slack hides future regressions.
- Hot paths (per-byte crypto loops, the inner loop of a fuzz harness) have stricter budgets and are exempt from logging at any level (cross-reference: `security.md` Logging).

## What is allowed in the name of performance

- **Move semantics over copies** for any non-trivial type. Pass by value when you intend to consume; otherwise `const&`.
- **Reserve before push** when the final size is known. Repeated `std::vector::push_back` with reallocations is a common, easy win.
- **`noexcept` on move** — the standard library's container reallocation falls back to copies when move is potentially throwing. This is one of the few places `noexcept` has a measurable performance effect.
- **Compile-time polymorphism (option 2 in `architecture.md`)** for ports on a measured hot path. The cost is testability — without virtual dispatch, GMock cannot mock the port (cross-reference: `tpm-write-tests`). Use this only when the runtime-dispatch cost is shown by benchmark to be material.
- **Static dispatch via `if constexpr`** for compile-time branches over algorithm identifiers, where every branch is exercised by a separate instantiation.

## What is not allowed in the name of performance

- **Skipping input validation at the public API.** The boundary check (`security.md`) is non-negotiable; the cost is dominated by the work that follows.
- **Skipping `secret_buffer` clearing.** The destructor's `OPENSSL_cleanse` is mandatory; "but the buffer goes out of scope soon anyway" is not an exception.
- **Replacing `CRYPTO_memcmp` with `memcmp`** because the latter is faster on equal-prefix paths. The whole point of `CRYPTO_memcmp` is the timing equality — speed differences *are* the bug.
- **Disabling sanitizer builds** in CI to "save time." The sanitizer matrix is a release blocker (`security.md`); skipping it is a release blocker.
- **Caching `EVP_*` or TSS2 contexts across threads** without locking. Cross-reference: `concurrency.md`. Per-thread caches are fine; cross-thread sharing without a mutex is undefined behavior.
- **Using `std::shared_ptr` where `std::unique_ptr` would do.** The atomic refcount is not free, and shared ownership is a design smell more often than a performance choice.

## Allocation discipline

- Hot paths allocate as little as possible. Prefer `std::array<uint8_t, N>` to `std::vector<uint8_t>` when N is known at compile time. Reserve when N is known at runtime.
- Small-buffer optimization is real but compiler-specific. Do not rely on it for correctness; do not write code that is broken when the SBO threshold changes.
- Avoid `std::stringstream` on hot paths. Concatenate into a pre-sized buffer or wait for `<format>` (C++20).
- Beware of unintended copies: `auto x = some_function()` may copy if the return type is implicitly converted, and range-based for over a non-reference yields copies. Use `auto&` or `const auto&` deliberately.

## Hot-path style

- Inline by default; the compiler's inliner is better than human judgment. `inline` annotations are about ODR, not speed. `[[gnu::always_inline]]` is reserved for measurement-driven cases with a comment explaining the measurement.
- Branch-prediction hints (`[[likely]]` / `[[unlikely]]`) only after profiling shows a mispredict-driven hot branch. They make code harder to read; the cost is real.
- Cache-line awareness matters when atomic counters are contended across threads. Pad them to 64 bytes (`alignas(64)`) when measurement shows false sharing — not preemptively.
- Avoid virtual dispatch in a per-byte loop. If the port is on the hot path, hoist the call above the loop or switch to compile-time polymorphism.

## Profiling

- Profiling tools: `perf` (Linux), Instruments (macOS), `callgrind` (cross-platform). Use what the host supports; do not write profiling output into the repository.
- Profile a release build with debug info (`-O3 -g -DNDEBUG`). Stripping symbols hides the hot frames; debug-build profiling is misleading.
- A flame graph that shows everything in `main` means symbols are missing — fix the build before drawing conclusions.
- Profile representative inputs. Crypto code on a 1 KB buffer has a different profile than on a 1 MB buffer; pick the size that matches the budget you care about.

## When the rules conflict with itself

If a benchmark contradicts this file (e.g., "compile-time polymorphism here was 0.2% — not worth it"), the benchmark wins for that case and the rationale is recorded in the PR. This file describes the default; measurements describe the reality.
