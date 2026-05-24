---
name: tpm-build-config
description: Build, tooling, and CI standards for the tpmkit C++17 library — CMake conventions, supported toolchains (GCC/Clang/MSVC), required compiler warning and hardening flags, sanitizer matrix (ASan/UBSan/TSan), clang-format/clang-tidy configuration, and vcpkg dependency management. Use when modifying CMakeLists, adding a third-party dependency, configuring CI, or changing compiler flags. Do not use for writing C++ source code, writing tests, or documentation work.
---

# Build and Tooling — tpmkit

This SKILL.md is the *policy* surface — required versions, flags, sanitizer matrix, vcpkg rules. For the *mechanics* of expressing that policy in CMake (target layout per the hexagonal architecture, export macro and visibility, install/export wiring for `find_package`, sanitizer interface targets, `CMakePresets.json`, vcpkg manifest), see `references/cmake-recipes.md`.

## Procedure

Apply this skill in order on every build, tooling, or CI change:

1. **Classify the change.** Identify whether it touches the build system, build-time options, toolchain set, compiler flags, sanitizers, coverage, static analysis, formatting, dependencies, reproducibility, or CI matrix. Each maps to a section below.
2. **Apply the matching policy section verbatim.** Do not invent flags, versions, or gates that are not listed here. If a needed value is missing, stop and surface the gap rather than guessing.
3. **Read `references/cmake-recipes.md`** when the change requires CMake mechanics (new target, export wiring, sanitizer interface target, invariant-check plumbing, vcpkg manifest entry).
4. **Re-run the invariant checks** before declaring the change done: domain isolation, header self-containment, symbol-export diff (see "Invariant checks" below). Treat any failure as a release blocker.
5. **Update the README and CMake cache documentation** if the change alters a supported version, a default, or a build-time option.

## Build system

- CMake **3.20** or later. Project requires `cxx_std_17` with `CXX_EXTENSIONS OFF` (no compiler-specific extensions).
- Out-of-source builds only. The repository's `.gitignore` excludes `build/`, `cmake-build-*/`.
- Targets follow the architecture layout (`.claude/rules/architecture.md`): one library target for the domain, one per adapter, plus a top-level interface target consumers link against.
- Provide a CMake package config so consumers can `find_package(<library> CONFIG)`. See `.claude/rules/library-api-design.md`.

## Build-time options

- `TPMKIT_LOG_MAX_LEVEL` — compile-time ceiling for log call sites. Default is `TRACE` for debug presets (everything compiled in for observability) and `INFO` for release presets (`DEBUG` and `TRACE` elided). Below the ceiling, the adapter's runtime filter takes over.
- Setting `TPMKIT_LOG_MAX_LEVEL` lower than the default in release is fine — it strengthens the elision guarantee for the never-log rules in `.claude/rules/security.md`. Setting it *higher* than the default in release weakens that backstop and requires a justification in the build configuration.
- Build-time options live in CMake cache variables, are documented in the README, and have defaults that produce a safe, hardened binary without further configuration.

## Supported toolchains

- GCC ≥ 9, Clang ≥ 10, MSVC ≥ 19.28 (Visual Studio 2019 16.8). Drop a compiler only with a documented reason.
- CI runs the test suite on each supported compiler, on Linux, macOS, and Windows.
- Document minimum versions in the README and keep them in sync with the CMake `cxx_std_17` requirement.

## Compiler flags

Required for every build:

- GCC/Clang: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2`.
- MSVC equivalent: `/W4 /permissive-`.
- `-Werror` (or `/WX`) is enabled in CI; warnings are tolerated in local dev builds for fast iteration only.

Required for release builds (cross-reference `.claude/rules/security.md` Build hardening):

- `-D_FORTIFY_SOURCE=2 -fstack-protector-strong -fstack-clash-protection -fPIE`
- `-Wl,-z,relro,-z,now,-z,noexecstack` on platforms that support it.

Forbidden:

- `-fpermissive`, `-fno-exceptions` (we throw), `-fno-rtti` (adapters use `dynamic_cast`), or any `-Wno-error=*` to silence specific warnings. Fix the warning instead.

## Sanitizers

- CI runs the test suite under each of: AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer.
- A sanitizer failure is a release blocker. Suppressions (`__attribute__((no_sanitize))`, `// NOLINT`) require a documented justification at the suppression site.
- Provide a CMake preset for each sanitizer. ASan and TSan cannot be linked together — one sanitizer per build.

**Running locally.** Reproduce a CI failure by invoking the matching preset:

- **ASan:** `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`
- **UBSan:** `cmake --preset ubsan && cmake --build --preset ubsan && ctest --preset ubsan`
- **TSan:** `cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan`

Set runtime options so the test exits on the first finding rather than continuing past a poisoned state:

- `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1`
- `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`
- `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1`

CTest presets bake these defaults in; override at the command line only when chasing a specific symptom.

## Code coverage

- Opt-in at configure time via `TPMKIT_COVERAGE` (default `OFF`). Debug builds only — never enabled in release.
- Coverage and sanitizer builds are mutually exclusive. CI runs coverage as its own job, not stacked with a sanitizer job.
- Coverage instrumentation is wired through a dedicated interface target alongside warnings, hardening, and sanitizers. Mechanics in `references/cmake-recipes.md`.
- Report generation (gcovr, `llvm-cov`, lcov) is a CI-pipeline concern downstream of the build; this skill does not prescribe a tool.

## Static analysis

- `clang-tidy` runs in CI with the project's `.clang-tidy`. New `clang-tidy` warnings are errors.
- `cppcheck` runs as a secondary check.
- Suppressions require an inline comment explaining why.

## Running analyzers locally

The analyzers that gate CI run locally against the project's `.clang-tidy` and cppcheck suppression list — there is no separate "local" config. Use these to catch issues before the CI round-trip.

**Prerequisite:** configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (enabled by default in the debug presets). Confirm `build/debug/compile_commands.json` exists before invoking any analyzer.

- **Whole-tree clang-tidy:** `run-clang-tidy -p build/debug/ -quiet` runs the project `.clang-tidy` across every translation unit in parallel. Use before opening a PR.
- **Changed lines only:** `git diff -U0 origin/main -- '*.cpp' '*.h' | clang-tidy-diff.py -p1 -path build/debug/` limits analysis to lines changed against the base branch. Use during iteration.
- **Single file:** `clang-tidy -p build/debug/ <path>` for targeted re-checks while fixing a warning.
- **cppcheck:** `cppcheck --project=build/debug/compile_commands.json --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1`. Slower than clang-tidy; run once clang-tidy is clean.

Output discipline:

- A new warning is treated the same locally as in CI — fix it, do not suppress. Suppressions follow the inline-justification rule in `.claude/rules/code-standards.md` Warnings.
- Use the project configs only. Do not pass `--checks=` or per-invocation overrides; if a check needs to change, change `.clang-tidy` in its own PR.
- After changing CMake options or adding files, reconfigure so `compile_commands.json` is regenerated before re-running analyzers.

## Invariant checks

Three structural rules from other policy files are enforced by automated checks in this build. Each is a release blocker, not an advisory:

- **Domain isolation** (`architecture.md`): no third-party headers (`openssl/`, `tss2/`, OS-specific) under `src/domain/` or `include/`. A grep-based CI gate fails the build on any match.
- **Header self-containment** (`library-api-design.md`): every public header under `include/` compiles standalone. The build generates one stub `.cpp` per header that includes only that header; the stub library must build clean with the project warning set.
- **Symbol export discipline** (`library-api-design.md`): symbols exported from the umbrella library are diffed against a checked-in baseline (`abi/exported-symbols.txt`). A change to the baseline is a deliberate PR with an ABI/SemVer note (cross-reference: `library-api-design.md` "Versioning and deprecation"). There is no auto-update mode.
- **Test policy discipline** (`tpm-write-tests`): `test_policy_guard` scans test sources for the minimum structural rules that are easy to regress: top-of-test behavior comments, parameterized contract tests, no deferred skip placeholders, and no compile-only header smoke tests under `tests/integration/`.

Mechanics for each in `references/cmake-recipes.md`.

## Formatting

- `clang-format` (≥ 14) with the project's `.clang-format`. CI fails on any unformatted file.
- A pre-commit hook runs `clang-format` on staged C++ files.

## Dependency management

- **Default:** vcpkg in manifest mode (`vcpkg.json`). All third-party dependencies pinned to a baseline commit.
- `FetchContent` is acceptable only for header-only libraries that are not in vcpkg.
- System packages (`apt`/`brew`) are not a supported development install path; they are an option for end users via the CMake package config.
- Adding a dependency requires a security review (license, maintenance status, CVE history) and a justification in the PR description.

## Reproducible builds

- The CMake build is deterministic given the same vcpkg baseline and compiler version. CI uses container images with pinned tool versions.
- `__DATE__`, `__TIME__`, and similar non-reproducible macros are not used in source.

## CI matrix and gates

The CI workflow is the single source of truth for what runs on every push and PR. The provider is a configuration choice (GitHub Actions, GitLab CI, etc.); the matrix and the gate split below are policy.

**Matrix dimensions** — every cell builds and tests green to allow merge:

- **Operating system:** Linux (Ubuntu LTS), macOS (current and previous major), Windows (Server 2022 / 2019).
- **Compiler:** GCC and Clang on Linux/macOS (versions per "Supported toolchains"), MSVC on Windows.
- **Build type:** Debug and Release.

**Required gates** — merge-blocking on every PR:

- Clean build with `-Werror` (`/WX` on MSVC) on every matrix cell.
- Test suite passes via `ctest --output-on-failure -j`.
- ASan, UBSan, and TSan jobs (one per sanitizer; see "Sanitizers" above).
- `clang-tidy` with the project `.clang-tidy`.
- `clang-format` check (no diff against the formatter).
- Invariant checks (domain isolation, header self-containment, symbol-export diff — see "Invariant checks" above).
- vcpkg manifest install with the pinned baseline.

**Advisory jobs** — run on every PR, do not block merge but are reviewed:

- Code coverage (`TPMKIT_COVERAGE=ON`) with the report uploaded as an artifact. Coverage trend is a review signal, not a gate.
- `cppcheck` secondary static analysis.

**Container and runner pinning:** CI uses container images pinned by digest, not tag, so a base-image rebuild does not silently change behavior. The pinned digests live in the workflow file; bumping them is a deliberate PR.

A pull request that disables, skips, or relaxes a required gate must include a justification in the PR description. Removing a gate requires a security review.

## Error Handling

* **Domain-isolation grep gate fails.** A third-party header reached `src/domain/` or `include/`. Move the include into the appropriate adapter folder (`src/adapters/<name>/`, or a grouped family such as `src/adapters/logging/<backend>/`), expose the capability via a port (`.claude/rules/architecture.md`), and re-run the gate. Do not relax the grep pattern.
* **Header self-containment build fails.** A public header depends on a transitive include. Add the missing `#include` to the header itself (or forward declare and move the include to the `.cpp`). Do not work around it by reordering includes in the stub.
* **Symbol-export diff fails.** The umbrella library's exported symbols drifted from `abi/exported-symbols.txt`. Decide deliberately: if the change is intentional, update the baseline in the same PR and add an ABI/SemVer note (`.claude/rules/library-api-design.md`); if unintentional, hide the symbol with the export macro and visibility rules in `references/cmake-recipes.md`.
* **Sanitizer job fails (ASan/UBSan/TSan).** Treat as a release blocker. Reproduce locally via the matching CMake preset before suppressing. A `__attribute__((no_sanitize))` or `// NOLINT` requires a justification comment at the suppression site (`.claude/rules/code-standards.md` Warnings).
* **`-Werror` build fails.** Fix the underlying type, lifetime, or dead-code issue. Do not add `-Wno-error=*` and do not cast to silence `-Wconversion`/`-Wsign-conversion` (`.claude/rules/code-standards.md`).
* **vcpkg manifest install fails.** Verify the baseline commit is reachable and the dependency exists at that baseline. Do not bump the baseline as a side effect — a baseline change is its own PR with a security review (`.claude/rules/security.md` Dependencies and CVE tracking).
* **Coverage and sanitizer requested together.** Reject the configuration. Run them as separate CI jobs as required by "Code coverage" above.
* **`test_policy_guard` fails after adding or moving tests.** Treat it as a test-layout bug. Fix the test file, tier, or contract shape instead of removing the guard from CTest.
* **Required policy value missing from this skill.** Stop and surface the gap to the maintainer rather than inventing a default. Adding a flag, gate, or supported version requires updating this SKILL.md first.
