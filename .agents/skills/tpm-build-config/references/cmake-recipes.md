# CMake recipes

Project policy — required versions, flags, sanitizer matrix, vcpkg rules — lives in this skill's `SKILL.md`. This file covers the *mechanics*: the concrete CMake patterns that wire tpmkit together correctly. The architecture (`.claude/rules/architecture.md`) and public-API rules (`.claude/rules/library-api-design.md`) constrain the shape; the recipes here implement that shape.

Modern CMake style applies throughout: target-based, no directory-scoped `include_directories`/`add_definitions`, no global `link_libraries`. Every property is attached to a target with explicit `PRIVATE`/`PUBLIC`/`INTERFACE`.

## One CMakeLists.txt per meaningful module

Build manifests mirror the architecture. Each module that consumers of the build graph care about — the domain, each adapter, composition, the tests root, examples — owns its own `CMakeLists.txt`. The top-level file declares the project, loads cross-cutting CMake modules, and stitches modules together with `add_subdirectory`; it does not define adapter targets, test executables, or source lists.

The boundary is "per meaningful module," not "per folder." A `src/adapters/openssl/utils/` directory whose `.cpp` files belong to the OpenSSL adapter target does not get its own `CMakeLists.txt` — the parent adds those sources via relative paths or `target_sources`. A thin aggregator such as `src/CMakeLists.txt` is acceptable when it keeps the repository root orchestration-only, but it should only dispatch to meaningful module manifests. A chain of one-line pass-through manifests below that point is noise; flatten it.

**Top-level `CMakeLists.txt` owns:** `project()`, `CMAKE_MODULE_PATH`, includes of `cmake/Tpmkit*.cmake`, top-level `add_subdirectory` calls, `enable_testing()`, and the conditional examples/tests/docs/install orchestration.

**`cmake/Tpmkit*.cmake` owns:** cache option validation, dependency discovery, reusable target option helpers, docs wiring, and install/export/package config wiring. These files should not list adapter sources or define test executables.

**Subdirectory `CMakeLists.txt` owns:** the `add_library(...)` (or `add_executable(...)`) call, the source list, `target_include_directories`, `target_link_libraries`, target-scoped compile definitions, and calls to `tpmkit_configure_library_target()` / `tpmkit_configure_executable_target()`.

Why this matters here specifically:

- **Mirrors the architecture.** The hexagonal rule says adapters are independent and depend only on the domain. Their build manifests should be independent for the same reason. A monolithic file makes adapter coupling look textual when it should look modular.
- **Bounded change-blast.** Adding a source file to the OpenSSL adapter touches one file owned by that adapter, not the project root. Diffs stay scoped to the layer the change belongs to.
- **Local reasoning.** Each manifest lists exactly the target's sources, includes, and dependencies. No scrolling past unrelated targets to find the one you are editing.

The recipes below assume this layout — each example is labelled with the file it belongs in.

## Target layout for hexagonal architecture

The folder layout in `architecture.md` maps to one CMake target per layer/adapter, plus the exported target consumers link against. Keep the target declaration in the module that owns the implementation and keep the root as a table of contents.

```cmake
# top-level CMakeLists.txt
include(TpmkitOptions)
include(TpmkitBuildOptions)
include(TpmkitDependencies)

add_subdirectory(src)

if(tpmkit_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

```cmake
# src/CMakeLists.txt
add_subdirectory(adapters/tpm2_esys)
add_subdirectory(adapters/mock)

if(TPMKIT_LOG_ADAPTER STREQUAL "spdlog")
    add_subdirectory(adapters/logging/spdlog)
endif()
```

Per-adapter `CMakeLists.txt` owns the target:

```cmake
# src/adapters/tpm2_esys/CMakeLists.txt
add_library(tpmkit)
add_library(tpmkit::tpmkit ALIAS tpmkit)

target_sources(tpmkit
    PRIVATE
        error_translation.cpp
        tcti_loader.cpp
        tpm_context.cpp
)
target_link_libraries(tpmkit
    PUBLIC
        Microsoft.GSL::GSL
        tl::expected
    PRIVATE
        PkgConfig::TSS2_ESYS
        PkgConfig::TSS2_TCTILDR
)
tpmkit_configure_library_target(tpmkit)
```

The key invariant: third-party libraries are **`PRIVATE`** dependencies of the adapter that uses them. Consumers of `tpmkit::tpmkit` never see `OpenSSL::Crypto` or `tss2::*` in their link line. If a consumer's compile fails because it can't find an `openssl/` header, the leak is in your `target_link_libraries` — promote the dependency back to `PRIVATE`.

As the project grows into separate domain/composition targets, preserve the same ownership rule: declare each target in its nearest meaningful module, then link the exported `tpmkit::tpmkit` target to the internal implementation targets without moving their source lists back to the root.

## Shared vs. static, and `BUILD_SHARED_LIBS`

Internal adapter and domain libraries stay `STATIC`. They are implementation detail of the umbrella `tpmkit::tpmkit` target and have no consumer use of their own — shipping them as separate shared objects multiplies install artifacts without buying anything.

The umbrella respects the standard `BUILD_SHARED_LIBS`. When it is built shared, the static adapter and domain libraries get linked into it and must be position-independent:

```cmake
set_target_properties(
    tpmkit_domain
    tpmkit_openssl_adapter
    tpmkit_tpm2_fapi_adapter
    tpmkit_tpm2_esys_adapter
    tpmkit_mock_adapter
    tpmkit_composition
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)
```

Equivalently, set `CMAKE_POSITION_INDEPENDENT_CODE ON` once in `cmake/TpmkitOptions.cmake` or attach `POSITION_INDEPENDENT_CODE` through the project option helper. Pick one place to set it; do not scatter the property.

If the umbrella becomes a shared library, set `VERSION` and `SOVERSION` on it so consumers get a stable runtime symlink layout:

```cmake
set_target_properties(tpmkit PROPERTIES
    VERSION   ${PROJECT_VERSION}        # e.g., 0.1.0
    SOVERSION ${PROJECT_VERSION_MAJOR}  # consumer link target — bump only on ABI break
)
```

`SOVERSION` follows the SemVer policy in `library-api-design.md` — bump on every major release. Do not force `BUILD_SHARED_LIBS` from the library; let the consumer (or preset) decide.

## Linking third-party deps that ship no CMake config

OpenSSL has an upstream CMake package (`find_package(OpenSSL REQUIRED)` → `OpenSSL::Crypto`). TPM2 TSS does not — vcpkg's port adds one, but a system install only ships `pkg-config` files. The portable pattern is `pkg-config` via `PkgConfig::*`:

```cmake
# src/adapters/tpm2_fapi/CMakeLists.txt
find_package(PkgConfig REQUIRED)
pkg_check_modules(TSS2_FAPI REQUIRED IMPORTED_TARGET tss2-fapi)

add_library(tpmkit_tpm2_fapi_adapter STATIC
    fapi_key_provider.cpp
)
target_link_libraries(tpmkit_tpm2_fapi_adapter
    PUBLIC  tpmkit_domain
    PRIVATE PkgConfig::TSS2_FAPI
)
```

`IMPORTED_TARGET` is the load-bearing keyword — it produces a real target you can link against, not loose `*_LIBRARIES`/`*_INCLUDE_DIRS` variables. Without it, you re-create the bug-prone `target_include_directories(... ${TSS2_FAPI_INCLUDE_DIRS})` ritual that target-based CMake exists to avoid.

The same pattern handles `tss2-esys`, `tss2-tcti-device`, etc. — one `pkg_check_modules` call per TSS library the adapter needs.

## The export macro and symbol visibility

Default visibility is hidden (`library-api-design.md`). One generated header defines the export macro; every public class or free function exported from the library is annotated.

```cmake
# in the domain target's CMakeLists.txt
include(GenerateExportHeader)

set_target_properties(tpmkit_domain PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

generate_export_header(tpmkit_domain
    BASE_NAME       TPMKIT
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/tpmkit/export.h
)
target_include_directories(tpmkit_domain
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
```

Generated `export.h` produces `TPMKIT_EXPORT`, `TPMKIT_NO_EXPORT`, etc. The convention in the codebase is to alias `TPMKIT_EXPORT` to `TPMKIT_API` in a single hand-written `include/tpmkit/api.h`:

```cpp
// include/tpmkit/api.h
#pragma once
#include "tpmkit/export.h"
#define TPMKIT_API TPMKIT_EXPORT
```

Class templates and inline functions are not annotated — they instantiate in the consumer's translation unit and exporting them across the ABI is undefined.

## Install and export for `find_package(tpmkit CONFIG)`

Three pieces must agree: the `install(TARGETS …)` call, the `install(EXPORT …)` call, and the `Config.cmake.in` template. Skip any one and consumers see "target not found" at `find_package` time.

```cmake
# top-level CMakeLists.txt, after the umbrella target
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS tpmkit tpmkit_domain tpmkit_composition
        EXPORT  tpmkitTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/tpmkit
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/tpmkit/export.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tpmkit
)

install(EXPORT tpmkitTargets
        FILE        tpmkitTargets.cmake
        NAMESPACE   tpmkit::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)

configure_package_config_file(
    cmake/tpmkitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/tpmkitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tpmkit
)
```

The `Config.cmake.in` template re-finds any **public** transitive dependencies — none, by design, for tpmkit, because adapters link OpenSSL/TSS privately:

```cmake
# cmake/tpmkitConfig.cmake.in
@PACKAGE_INIT@

# No find_dependency() calls — all third-party deps are PRIVATE to adapters.
# If you ever add one here, you've leaked it into the public ABI; fix that first.

include("${CMAKE_CURRENT_LIST_DIR}/tpmkitTargets.cmake")
check_required_components(tpmkit)
```

If a `find_dependency(OpenSSL)` call sneaks into this template, treat it as a bug report against `target_link_libraries` somewhere — a `PRIVATE` got typed as `PUBLIC`.

## The composition root linkage

The composition root is the **only** target that names every adapter. It picks at build time (OpenSSL backend via `LIB_CRYPTO_BACKEND`) or compiles all adapters in for runtime selection (TPM2 TSS).

```cmake
# src/composition/CMakeLists.txt
add_library(tpmkit_composition STATIC composition_root.cpp)
target_link_libraries(tpmkit_composition
    PUBLIC  tpmkit_domain
    PRIVATE
        tpmkit_openssl_adapter      # build-time choice — see below
        tpmkit_tpm2_fapi_adapter    # all TPM adapters compiled in
        tpmkit_tpm2_esys_adapter
        tpmkit_mock_adapter
)
```

For the OpenSSL build-time switch:

```cmake
set(LIB_CRYPTO_BACKEND "openssl" CACHE STRING "Crypto backend (openssl|...)")
set_property(CACHE LIB_CRYPTO_BACKEND PROPERTY STRINGS openssl)

if(LIB_CRYPTO_BACKEND STREQUAL "openssl")
    target_link_libraries(tpmkit_composition PRIVATE tpmkit_openssl_adapter)
else()
    message(FATAL_ERROR "Unknown LIB_CRYPTO_BACKEND: ${LIB_CRYPTO_BACKEND}")
endif()
```

No silent default fall-through — unknown backend is a fatal error, matching the "fail closed" rule in `security.md`.

## Build-time options

Build-time options use `option()` for booleans and `set(... CACHE ...)` for everything else. They are surfaced as compile definitions on the affected targets, not as global `add_compile_definitions`.

```cmake
set(TPMKIT_LOG_MAX_LEVEL "" CACHE STRING
    "Compile-time log ceiling (TRACE|DEBUG|INFO|WARN|ERROR). Empty = preset default.")

if(TPMKIT_LOG_MAX_LEVEL)
    target_compile_definitions(tpmkit_domain
        PUBLIC TPMKIT_LOG_MAX_LEVEL=TPMKIT_LOG_LEVEL_${TPMKIT_LOG_MAX_LEVEL}
    )
endif()
```

`PUBLIC` here because the macro affects code that gets instantiated in consumer translation units (the logging call-site elision lives in headers). If it were a purely internal switch, `PRIVATE` would be correct.

## Consumed as a subdirectory

Downstream projects may vendor tpmkit by `add_subdirectory(third_party/tpmkit)` instead of `find_package`. When they do, the build must not impose tests, examples, install rules, or compiler-standard overrides on the parent project.

```cmake
# top-level CMakeLists.txt
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(TPMKIT_IS_TOP_LEVEL ON)
else()
    set(TPMKIT_IS_TOP_LEVEL OFF)
endif()

option(TPMKIT_BUILD_TESTS    "Build the test suite"   ${TPMKIT_IS_TOP_LEVEL})
option(TPMKIT_BUILD_EXAMPLES "Build the examples"     ${TPMKIT_IS_TOP_LEVEL})
option(TPMKIT_INSTALL        "Generate install rules" ${TPMKIT_IS_TOP_LEVEL})

if(TPMKIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(TPMKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(TPMKIT_INSTALL)
    # the install(...) and install(EXPORT ...) calls
endif()
```

`CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME` is the CMake-3.20-compatible equivalent of `PROJECT_IS_TOP_LEVEL` (added in 3.21). Once the project minimum moves to 3.21, swap to the built-in.

Never set `CMAKE_CXX_STANDARD` at top level — that overrides the parent's choice. Use `target_compile_features(<target> PUBLIC cxx_std_17)` so the requirement propagates to consumers without imposing a global default. Same logic applies to `CMAKE_CXX_EXTENSIONS`: set it as a target property (`set_target_properties(... CXX_EXTENSIONS OFF)`), not as a directory-scoped variable.

## Centralizing compiler and hardening flags

Keep project-wide flags in `cmake/TpmkitBuildOptions.cmake` and apply them through target-scoped helper functions. This avoids both `add_compile_options` (directory-scoped, affects subdirectories silently) and per-target flag duplication.

```cmake
# cmake/TpmkitBuildOptions.cmake
function(tpmkit_configure_library_target target)
    target_compile_features("${target}" PUBLIC cxx_std_17)
    set_target_properties("${target}" PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    tpmkit_apply_project_options("${target}")
endfunction()
```

Then, in each library or executable target manifest:

```cmake
tpmkit_configure_library_target(tpmkit)
tpmkit_configure_executable_target(tpmkit_unit_public_api_tests)
```

The helper functions apply warning, hardening, sanitizer, and coverage options directly to the target being built. Do not use directory-scoped `include_directories`, `add_definitions`, `add_compile_options`, or `add_link_options` for project policy. If a future refactor uses interface targets instead, keep the same rule: link them explicitly to each target and never make the root `CMakeLists.txt` the owner of module source lists.

## Sanitizer presets

Sanitizers are mutually exclusive (ASan and TSan can't be combined). One preset per sanitizer, never a "kitchen sink" target.

```cmake
if(tpmkit_USE_ASAN)
    target_compile_options("${target}" PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    if(tpmkit_can_use_link_options)
        target_link_options("${target}" PRIVATE -fsanitize=address)
    endif()
endif()
```

These attach through `tpmkit_apply_project_options()` and the preset-backed cache options (`tpmkit_USE_ASAN`, `tpmkit_USE_UBSAN`, `tpmkit_USE_TSAN`). Keep the mutual-exclusion validation in `cmake/TpmkitOptions.cmake`, not in individual target manifests.

## Code coverage

Coverage is opt-in at configure time, off by default, and Debug-only. It is applied by `tpmkit_apply_project_options()` to each configured target, alongside sanitizer flags.

```cmake
option(TPMKIT_COVERAGE "Build with code coverage instrumentation" OFF)

if(TPMKIT_COVERAGE)
    target_compile_options("${target}" PRIVATE --coverage -O0 -g)
    if(tpmkit_can_use_link_options)
        target_link_options("${target}" PRIVATE --coverage)
    endif()
endif()
```

GCC uses gcov-style `.gcno`/`.gcda` files; Clang uses source-based `.profraw`. Different flag sets, different output formats — pick once per build, do not mix.

Apply the project option helper to **both** the library targets and the test executables:

```cmake
tpmkit_configure_library_target(tpmkit)
tpmkit_configure_executable_target(tpmkit_unit_tpm2_esys_tests)
```

Test attachment matters because data files are written when the *test binary* runs — without instrumenting the test executable, the runtime support is missing and no data lands on disk.

Constraints:

- **No optimization.** Coverage with `-O2`/`-O3` produces misleading line accounting (inlining, dead-store elimination). The recipe forces `-O0`. Use Debug presets only.
- **Mutually exclusive with sanitizers.** Combining coverage with TSan is unsupported; with ASan it technically works but mixes unrelated signals. CI runs coverage as its own job, not stacked with a sanitizer job.
- **Never in release.** The hardening flags in `tpmkit_hardening` are `$<CONFIG:Release>`-guarded; enabling `TPMKIT_COVERAGE` in a release build is a configuration error and should be rejected up front by the preset choice, not by a runtime check.

Report generation (gcovr, `llvm-cov export`, lcov) reads the data files after `ctest` runs. The choice of tool is a CI-pipeline concern downstream of the build — out of scope for this file.

## Test wiring with CTest and GoogleTest

`enable_testing()` belongs in the **top-level** `CMakeLists.txt` only. CTest searches up from the build root and stops at the first `CMakeCache.txt` — calling `enable_testing()` from a subdirectory leaves the build root with no test registry, and `ctest` reports zero tests.

Once enabled, each test directory wires its targets through `gtest_discover_tests`, which queries the test binary at build time and registers each `TEST(...)` case as a separate CTest entry:

```cmake
# tests/domain/CMakeLists.txt
include(GoogleTest)

add_executable(tpmkit_domain_tests
    digest_sha256_test.cpp
    signature_test.cpp
)
target_link_libraries(tpmkit_domain_tests
    PRIVATE
        tpmkit_domain
        tpmkit_mock_adapter
        GTest::gtest_main
)
tpmkit_configure_executable_target(tpmkit_domain_tests)
gtest_discover_tests(tpmkit_domain_tests
    PROPERTIES TIMEOUT 30
)
```

Prefer `gtest_discover_tests` over `gtest_add_tests` (which parses sources at configure time and breaks when test names are computed) and over a single `add_test(NAME all COMMAND tpmkit_domain_tests)` (which collapses every failure into one CTest entry). Per-test entries give CTest's `--rerun-failed` and `-j` parallel scheduling something to work with.

Run with `ctest --test-dir build/<preset> --output-on-failure -j`. CI uses the same invocation.

## Invariant checks (architecture, headers, ABI)

Three structural invariants from other policy files are enforced as build-integrated checks (policy: SKILL.md "Invariant checks"). Each is wired so a developer's local `ctest` run sees the same failure CI does — the invariants are not CI-only gates.

### Domain isolation grep

Architecture rule: domain code includes no third-party headers. A small script fails the build if `git grep` finds a forbidden include under the domain folder.

```cmake
# tests/architecture/CMakeLists.txt
add_test(NAME domain_isolation
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}
            sh -c "! git grep -lE '^#include[[:space:]]+[<\"](openssl|tss2)/' -- 'src/domain/*' 'include/**/*'"
)
set_tests_properties(domain_isolation PROPERTIES LABELS "architecture")
```

The `!` makes the test pass only when grep finds nothing. On Windows, swap `sh -c` for a small `cmake -P` script that wraps the same logic.

### Header self-containment

Library rule: every installed header compiles standalone. The recipe generates one stub `.cpp` per public header at configure time, then builds a single object library from those stubs:

```cmake
# tests/headers/CMakeLists.txt
file(GLOB_RECURSE PUBLIC_HEADERS
    RELATIVE ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/tpmkit/*.h
)

set(STUB_DIR ${CMAKE_CURRENT_BINARY_DIR}/header_stubs)
file(MAKE_DIRECTORY ${STUB_DIR})

set(STUB_SOURCES "")
foreach(header ${PUBLIC_HEADERS})
    string(REPLACE "/" "_" stub_name ${header})
    string(REPLACE "." "_" stub_name ${stub_name})
    set(stub_path ${STUB_DIR}/${stub_name}.cpp)
    file(WRITE ${stub_path} "#include \"${header}\"\n")
    list(APPEND STUB_SOURCES ${stub_path})
endforeach()

add_library(tpmkit_header_self_containment OBJECT ${STUB_SOURCES})
target_link_libraries(tpmkit_header_self_containment
    PRIVATE tpmkit::tpmkit
)
tpmkit_apply_project_options(tpmkit_header_self_containment)
```

Link only `tpmkit::tpmkit` (and the warning interface) — *not* OpenSSL or TSS. The point of the test is that the public header brings in everything it needs on its own. If a stub fails because `tss2/...` is missing, the public header is leaking an adapter detail and the fix is to forward-declare or move the include into the `.cpp`, not to add the dep to the stub library.

The build-step failure is the signal — no runtime test needed.

### Exported-symbol diff

ABI rule: changes to the exported symbol set are deliberate. Dump the exported symbols from the built umbrella library and diff against a baseline checked into `abi/exported-symbols.txt`:

```cmake
# tests/abi/CMakeLists.txt
add_test(NAME exported_symbols_diff
    COMMAND ${CMAKE_COMMAND}
        -DLIB_FILE=$<TARGET_FILE:tpmkit>
        -DBASELINE=${CMAKE_SOURCE_DIR}/abi/exported-symbols.txt
        -P ${CMAKE_SOURCE_DIR}/cmake/check_exported_symbols.cmake
)
set_tests_properties(exported_symbols_diff PROPERTIES LABELS "abi")
```

The helper script (`cmake/check_exported_symbols.cmake`) runs `nm -D --defined-only --extern-only "${LIB_FILE}"` (or `dumpbin /EXPORTS` on MSVC), normalizes the output (strip addresses, sort), and `diff`s against `${BASELINE}`. A non-empty diff fails the test. The expected fix is one of:

- The symbol shouldn't have been exported — annotate with `TPMKIT_NO_EXPORT` or move the definition out of the public header.
- The symbol *should* be exported and the baseline is now stale — update `abi/exported-symbols.txt` in the same PR, and bump SemVer per `library-api-design.md` if the change is breaking.

The script has no `--update` flag. Baseline changes are always a deliberate human review — automating the update defeats the purpose of the gate.

## CMake presets

`CMakePresets.json` (CMake ≥ 3.20) is the single source of truth for build configurations. Every preset wires the vcpkg toolchain explicitly so contributors don't need environment variables.

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build/${presetName}",
      "generator": "Ninja",
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": { "CMAKE_EXPORT_COMPILE_COMMANDS": "ON" }
    },
    { "name": "debug",   "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug"          } },
    { "name": "release", "inherits": "base", "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" } },
    { "name": "asan",    "inherits": "debug","cacheVariables": { "tpmkit_USE_ASAN": "ON"              } },
    { "name": "ubsan",   "inherits": "debug","cacheVariables": { "tpmkit_USE_UBSAN": "ON"             } },
    { "name": "tsan",    "inherits": "debug","cacheVariables": { "tpmkit_USE_TSAN": "ON"              } }
  ]
}
```

`CMAKE_EXPORT_COMPILE_COMMANDS=ON` produces `compile_commands.json` in the build directory — required by `clang-tidy`, `clangd`, and IDE indexers (cross-reference: SKILL.md "Static analysis"). Set it in the base preset so every build benefits without per-developer configuration.

CI uses the same presets contributors run locally — no separate "CI build" exists.

## vcpkg manifest

`vcpkg.json` lives at the repo root. The `builtin-baseline` is the pinned commit from the security policy in `.claude/rules/security.md` ("never depend on latest").

```json
{
  "name": "tpmkit",
  "version-string": "0.1.0",
  "builtin-baseline": "<commit-sha-from-vcpkg-repo>",
  "dependencies": [
    "openssl",
    { "name": "tpm2-tss", "platform": "linux" },
    { "name": "ms-gsl"  },
    { "name": "gtest",     "host": false },
    { "name": "rapidcheck","host": false }
  ],
  "overrides": [
    { "name": "openssl",  "version": "3.2.1" },
    { "name": "tpm2-tss", "version": "4.1.0" }
  ]
}
```

`overrides` pin exact versions. Bumping them is a deliberate PR with a security-review note (the security rule applies to upgrades, not just initial selection).

## Generator expressions used in this file

Generator expressions are evaluated when the build files are written, not when CMake is parsed, so they can branch on configuration, target properties, and platform. The ones used in the recipes above:

- `$<BUILD_INTERFACE:path>` / `$<INSTALL_INTERFACE:path>` — pick the right include directory depending on whether the target is being built locally or consumed from an install tree. Required for `target_include_directories` on any installed target.
- `$<CONFIG:Release>` — true for the `Release` build configuration. Used to attach hardening flags only to release builds without polluting debug.
- `$<PLATFORM_ID:Linux>` / `$<PLATFORM_ID:Darwin>` / `$<PLATFORM_ID:Windows>` — true for the named platform. Used to gate platform-specific link options (`-Wl,-z,relro` is GNU ld only).
- `$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>` — true when compiling C++ with GCC or Clang. Prefer over `if(CMAKE_CXX_COMPILER_ID MATCHES …)` because it scopes to the target and to the language being compiled, which matters when a target later picks up `.c` sources.
- `$<AND:expr1,expr2>` / `$<OR:expr1,expr2>` / `$<NOT:expr>` — combine conditions. `$<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Linux>>:flag>` is the idiomatic shape for "release on Linux only."

If a property is the same across all configurations and platforms, use a plain string — generator expressions only pay off when there is a real branch.

## Common mistakes

- **`PUBLIC` link to OpenSSL/TSS from an adapter.** Leaks the third-party header into the public ABI, defeats hexagonal isolation, and breaks `find_package(tpmkit CONFIG)` for any consumer that doesn't independently install OpenSSL. Use `PRIVATE`.
- **`include_directories()` at directory scope.** Affects every target in the directory, including ones you didn't intend (e.g., a test target inheriting an adapter's private includes). Always `target_include_directories(target PRIVATE …)`.
- **Public include dirs without `BUILD_INTERFACE`/`INSTALL_INTERFACE`.** The install tree path differs from the build tree. Without the generator expressions, the installed package config points at a build directory that no longer exists on the consumer's machine.
- **Forgetting `install(EXPORT …)`.** `install(TARGETS … EXPORT name)` registers the target for export; `install(EXPORT name …)` writes the actual `.cmake` file. Both are required.
- **Missing `NAMESPACE tpmkit::`.** Without it, consumers must write `tpmkit_domain` directly. With it, they write `tpmkit::tpmkit` and get a clear error if they forget `find_package`.
- **`add_compile_options` instead of the project option helper.** Affects unrelated targets in the same directory and silently changes subdirectory behavior. Centralize in `cmake/TpmkitBuildOptions.cmake` and call `tpmkit_configure_library_target()` / `tpmkit_configure_executable_target()` explicitly.
- **Hand-defining the export macro.** `generate_export_header` handles MSVC/GCC/Clang differences correctly. A handwritten `__attribute__((visibility))` macro will get a corner case wrong on at least one platform.
- **Shipping `find_dependency(OpenSSL)` in `Config.cmake.in`.** This is the diagnostic, not the fix. Find the `target_link_libraries` line that should have been `PRIVATE` and change it.
- **`CMAKE_CXX_FLAGS += "-fsanitize=…"`.** Pollutes every target globally and silently breaks targets (e.g., release-only tools) you didn't intend. Use the sanitizer branch in `tpmkit_apply_project_options()`.
- **Mixing ASan and TSan in one build.** They are incompatible. Use separate presets and run them as separate CI jobs.
- **Calling `enable_testing()` from a subdirectory.** CTest only registers the test directory rooted at the cache file. The call belongs in the top-level `CMakeLists.txt`; subdirectories use `gtest_discover_tests` to add entries.
- **`pkg_check_modules(... tss2-fapi)` without `IMPORTED_TARGET`.** Produces loose `*_LIBRARIES`/`*_INCLUDE_DIRS` variables and forces `target_include_directories` everywhere. With `IMPORTED_TARGET`, you link `PkgConfig::TSS2_FAPI` and the includes propagate.
- **`set(CMAKE_CXX_STANDARD 17)` at top level.** Imposes the standard on parent projects when tpmkit is added via `add_subdirectory`. Use `target_compile_features(<target> PUBLIC cxx_std_17)` and let the requirement propagate through link dependencies.
- **`set(BUILD_SHARED_LIBS ON)` inside the library.** Overrides the consumer's choice. Leave `BUILD_SHARED_LIBS` to the consumer or to the preset.
- **Using `gtest_add_tests` instead of `gtest_discover_tests`.** The former parses sources at configure time and silently misses parameterised or computed test names.
- **Combining `TPMKIT_COVERAGE` with a release preset or a sanitizer.** Coverage requires `-O0` and Debug; release hardening assumes optimization, and sanitizer signals get tangled with coverage data. One instrumentation per build.
- **Forgetting to apply project options to test executables.** Library-only coverage instrumentation produces no `.gcda`/`.profraw` files because the data is written by the running test binary.
- **Linking adapter dependencies (OpenSSL, TSS) into the header self-containment stub library.** Defeats the test — the point is that public headers are self-contained without those deps. If a stub fails compilation, fix the header, not the stub library's link line.
- **Adding an `--update` flag to the exported-symbol diff script.** Removes the human-in-the-loop check that the gate exists to enforce. Baseline updates are PR-reviewed, never automated.
- **Putting the invariant checks only in CI.** They are wired as `add_test` entries so a developer's local `ctest` sees the same failure. A "passes locally, fails in CI" gap on these gates is itself a process failure.
