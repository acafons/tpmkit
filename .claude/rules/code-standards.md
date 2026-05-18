# C++17 Code Standards

This file is the project's opinionated take. For language-level questions not covered here, the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) are the external reference — cite the rule ID (P.4, F.51, C.21, …) when applying one. Where a Core Guidelines rule conflicts with this file, this file wins; raise the conflict in review so we can decide whether to update.

## Language and naming

- All code (identifiers, comments, commit messages) must be written in English.
- Use `snake_case` throughout — functions, methods, variables, parameters, `constexpr` constants, classes, structs, enums, and type aliases — aligning with the C++ standard library.
- Use single uppercase letters (`T`, `U`, `K`, `V`) or `PascalCase` (`Key`, `Allocator`, `RandomIt`) for template parameters only — STL convention for templates.
- Use `snake_case` for file and folder names (e.g., `order_service.cpp`, `order_service.h`).
- Suffix non-static class data members with a trailing underscore (e.g., `count_`, `connection_`) to distinguish them from local variables and parameters.
- Do not prefix identifiers with underscores or use double underscores anywhere — those are reserved by the standard.
- Macros (avoid them when possible) use `SCREAMING_SNAKE_CASE`.
- `enum class` values use `snake_case` (matching `std::endian::little`, `std::float_round_style::round_to_nearest`).
- Avoid abbreviations, and avoid names longer than 30 characters.

## Functions and methods

- Function and method names must begin with a verb and describe a single, well-defined action (e.g., `calculate_total`, `load_config`).
- Keep functions short — prefer under 20 lines. Split when they grow beyond that.
- Prefer no more than 3 parameters. When more are needed, group them into a struct or parameter object.
- Avoid boolean parameters that switch behavior. Prefer separate functions, overloads, or `enum class` flags.
- Follow Command-Query Separation: a function either mutates state or returns a value, never both. Mark queries `[[nodiscard]]` and `const`.
- Mark functions that cannot throw as `noexcept` when it is part of the contract (move constructors, swap, destructors).
- Prefer early returns; avoid `else` after `return`. Do not nest more than two conditional levels.

## Variables and constants

- Declare each variable on its own line.
- Declare variables as close as possible to where they are first used.
- Default to `const` and `constexpr`; mutability should be opt-in.
- Replace magic numbers with named `constexpr` constants.
- Prefer `auto` only when the type is obvious from the right-hand side or would be noisy to spell out; otherwise be explicit.

## Classes and types

- Keep classes focused — prefer under 300 lines. Split responsibilities when they grow.
- Follow the Rule of Zero by default. If you must declare any of the special members, follow the Rule of Five.
- Mark single-argument constructors `explicit` unless implicit conversion is intentional.
- Prefer composition over inheritance. Use `final` on classes and virtual methods that should not be extended further.
- Use `enum class` instead of plain `enum`.
- Prefer `struct` for passive data aggregates (all members public, no invariants); use `class` when invariants must be enforced.
- Initialize members in-class or via constructor member initializer lists — never assign in the constructor body when initialization will do.
- Avoid two-phase initialization. A constructor must leave the object in a fully valid, usable state — no separate `init()`/`open()`/`load()` step required before use. If construction can fail, throw from the constructor, or expose a static factory returning `std::optional<T>`.
- Prefer free functions in an anonymous namespace (in the `.cpp`) over private methods when the helper does not need access to non-static private state. Private methods clutter the header, contribute to the class's surface for change-impact purposes, and complicate Pimpl/`final`/inheritance reasoning. Free functions in `.cpp` are translation-unit-private, do not appear in the public symbol table, and remain testable via the public API alone.
- Prefer value objects over primitive types for any domain value that carries meaning (key handles, digests, signatures, algorithm identifiers, byte counts with semantics). A value object is immutable, validates its invariants in the constructor, compares by value, and exposes no mutators. Use raw `uint32_t`, `std::vector<uint8_t>`, or `std::string` only for true primitives with no domain semantics.
- **Access block order.** Declare access blocks in the order `public:`, `protected:`, `private:` — at most one of each. The public interface comes first because that is what consumers read; private members come last because they are implementation detail. Do not interleave access blocks (`public: ... private: ... public:` zigzag).
- **Member order.** With the exception of the constructor/destructor group, members are alphabetised. Within each access block:
  1. Type aliases and nested types, alphabetised.
  2. Static constants and `constexpr` members, alphabetised.
  3. The constructor/destructor group, in this order: default constructor, other constructors (declaration order), destructor, copy constructor, copy assignment, move constructor, move assignment.
  4. Methods, alphabetised. Static methods alphabetised among instance methods.
  5. Operators as a single contiguous block, alphabetised within (`operator()`, `operator<<`, `operator==`, …).
  6. Data members, alphabetised. Almost always in `private:`.
  7. Friend declarations, at the end.

  For overloads that differ only by `const`-correctness, declare non-`const` first.

## Memory and resources

- Use RAII for every resource (memory, files, sockets, locks).
- No raw `new`/`delete`. Use `std::make_unique` and `std::make_shared`.
- Pass non-trivial types by `const&`; pass by value when you intend to copy or move. Do not use arguments as output in project signatures — return the value, a struct, `std::tuple`, or `outcome<T, error>` (`error-handling.md`). C libraries that take out-pointers (`TSS2_*`, `EVP_*`) are called *behind* a project signature that still returns its result; do not let the out-parameter shape leak into project code.
- Use `std::optional` instead of sentinel values, and `std::variant` instead of tagged unions.
- Use `std::string_view` and `gsl::span` (or an internal `byte_span` type — `std::span` is C++20) for non-owning parameters when lifetimes are clear.
- For byte buffers specifically: `std::array<std::uint8_t, N>` for fixed-size buffers (digests, fixed-layout headers), `std::vector<std::uint8_t>` for dynamic buffers (parsed messages, variable-length signatures), `gsl::span<const std::uint8_t>` (or the project's `byte_span`) for non-owning views. Raw pointers or C arrays only when crossing into a C-library API — wrap them in a span at the earliest opportunity. **Secret bytes always use `secret_buffer`** (`security.md`), never `std::vector<std::uint8_t>` or `std::string`.
- When wrapping resources from C libraries, use `std::unique_ptr` with a custom deleter for one-off cases, or a dedicated RAII wrapper class (with deleted copy and defined move) when the resource is used in many places. Never rely on manual cleanup calls at use sites.

## Headers and modules

- Use `.h` for header files (not `.hpp`) and `.cpp` for translation units. Pick one and apply project-wide.
- Use `#pragma once` at the top of every header.
- Forward declare in headers when possible; include in implementation files.
- Never write `using namespace` at namespace scope in a header.
- Keep template and `constexpr` definitions that must be visible in headers; everything else belongs in the `.cpp` file.
- Group includes in order: corresponding header, project headers, third-party, standard library — separated by blank lines.

## Warnings

- Compile cleanly under the project's warning set. CI rejects any warning — don't introduce code that emits one.
- Do not disable a warning to silence it. Fix the underlying type mismatch, lifetime issue, or dead code.
- Suppressions (`#pragma GCC diagnostic`, `// NOLINT`, `__attribute__((unused))`) require an inline comment explaining why. Temporary suppressions reference a ticket.
- Casts whose only purpose is to silence `-Wconversion` or `-Wsign-conversion` are a smell — the type is wrong; fix the type.

## Error handling

- Prefer exceptions for exceptional conditions; prefer `std::optional` or a result type for expected failures.
- Do not silently swallow exceptions. Catch by `const&`.
- Validate inputs at API boundaries; trust internal callers.

## Comments

- Avoid redundant comments that restate what the code already says.
- Do write comments for non-obvious invariants, ownership and lifetime contracts, threading assumptions, `noexcept` rationale, and any place where behavior would surprise a reader.
- Prefer expressing intent through names and types over explaining it in prose.
- Tests are the exception to the minimal-comment default: every GoogleTest `TEST`, `TEST_F`, and `TEST_P` body starts with one short `//` comment naming the behavior under test, then keeps Arrange / Act / Assert blocks visually distinct.
