# tpmkit

Status: experimental pre-1.0

tpmkit is a C++17 library that wraps TPM2 TSS ESYS behind a small, backend-neutral API. The first public surface centers on `tpmkit::tpm_context`, which owns TCTI setup, ESYS initialization, optional TPM startup, and RAII teardown while returning domain errors instead of raw `TSS2_RC` values.

## Quickstart

Build the debug preset and run the example inside the dev container. The helper starts an IBM software TPM and `tpm2-abrmd`; use the resource-manager TCTI `tabrmd:bus_type=system` for commands that run while the broker is active.

```sh
./scripts/run-tpmkit-docker.sh
./scripts/exec-tpmkit-docker.sh tpm-start.sh
./scripts/exec-tpmkit-docker.sh 'cmake --preset debug'
./scripts/exec-tpmkit-docker.sh 'cmake --build --preset debug'
./scripts/exec-tpmkit-docker.sh './build/debug/examples/tpm_context_basics "tabrmd:bus_type=system"'
./scripts/exec-tpmkit-docker.sh './build/debug/examples/pcr_read_sha256 "tabrmd:bus_type=system"'
```

The example sources live under [examples/](examples/), including:
[tpm_context_basics.cpp](examples/tpm_context_basics.cpp),
[pcr/active_banks.cpp](examples/pcr/active_banks.cpp),
[pcr/read_sha256.cpp](examples/pcr/read_sha256.cpp),
[pcr/read_all_sha256.cpp](examples/pcr/read_all_sha256.cpp),
[pcr/reset_debug.cpp](examples/pcr/reset_debug.cpp),
[pcr/extend_debug.cpp](examples/pcr/extend_debug.cpp),
[pcr/event_debug.cpp](examples/pcr/event_debug.cpp), and
[pcr/observer_trace.cpp](examples/pcr/observer_trace.cpp).

PCR headers are grouped under `<tpmkit/pcr/...>`.

## TCTI Selection

tpmkit requires callers to pass an explicit TCTI string or an owned TCTI handle. It does not guess from environment or platform defaults. The rationale is documented in [ADR-002](.compozy/tasks/esys-context/adrs/adr-002.md).

| TCTI | Typical use | Example |
| --- | --- | --- |
| `mssim:` | IBM simulator when no resource manager has already claimed it | `mssim:host=127.0.0.1,port=2321` |
| `swtpm:` | swtpm socket or TCP simulator setups | `swtpm:host=127.0.0.1,port=2321` |
| `device:` | Linux hardware TPM or resource manager device | `device:/dev/tpmrm0` |
| `tabrmd:` | TPM2 access broker via D-Bus | `tabrmd:bus_type=system` |

Example invocation with a swtpm TCTI:

```sh
./build/debug/examples/tpm_context_basics "swtpm:host=127.0.0.1,port=2321"
```

## Building

Dependencies are pinned in [vcpkg.json](vcpkg.json): `tl-expected` 1.3.1, `ms-gsl` 4.2.1, `tpm2-tss` 4.1.3, `spdlog` for the optional spdlog logger adapter, and `gtest` 1.17.0 port 2. Local project commands that use CMake, the TPM stack, or the toolchain must run through the dev container.

```sh
./scripts/run-tpmkit-docker.sh
./scripts/exec-tpmkit-docker.sh 'cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DTPMKIT_BUILD_EXAMPLES=ON'
./scripts/exec-tpmkit-docker.sh 'cmake --build build/debug'
./scripts/exec-tpmkit-docker.sh 'ctest --test-dir build/debug --output-on-failure'
```

Supported presets are `debug`, `release`, `asan`, `ubsan`, `tsan`, and `coverage`.

## Installing

Install the library and CMake package files with:

```sh
./scripts/exec-tpmkit-docker.sh 'cmake --install build/debug --prefix /tmp/tpmkit-install'
```

Production installs export `tpmkit::tpmkit`. Test substitute headers and the `tpmkit::tpmkit_testing` target are installed only when `TPMKIT_INSTALL_TESTING=ON`; see [ADR-008](.compozy/tasks/esys-context/adrs/adr-008.md). Optional logger adapter targets are installed only when selected with `TPMKIT_LOG_ADAPTER`; downstream consumers request them with `find_package(tpmkit CONFIG REQUIRED COMPONENTS spdlog)` or `find_package(tpmkit CONFIG REQUIRED COMPONENTS stdio)` and link `tpmkit::tpmkit_spdlog` or `tpmkit::tpmkit_stdio`.

## Logging

tpmkit logs through the `tpmkit::logger` port. Consumers wire a concrete adapter at their composition root through `tpm_context_config::log`; leaving it unset uses `noop_logger` at runtime.

Logger adapter selection is controlled by the CMake cache `STRING` `TPMKIT_LOG_ADAPTER`; see [ADR-001](.compozy/tasks/stdio-logger-adapter/adrs/adr-001.md).

| Value | Result |
| --- | --- |
| `none` | Default. No external logger adapter target is built or installed; no external logging dependency is pulled in. Runtime logging still works through `noop_logger` and emits nothing unless a caller supplies its own adapter. See [ADR-002](.compozy/tasks/stdio-logger-adapter/adrs/adr-002.md). |
| `spdlog` | Builds and installs `tpmkit::tpmkit_spdlog`, which adapts records to spdlog. Consumers link this target and include `<tpmkit/logging/spdlog_logger.h>`. |
| `stdio` | Builds and installs `tpmkit::tpmkit_stdio`, a zero-dependency adapter for stdout/stderr. Consumers link this target and include `<tpmkit/logging/stdio_logger.h>`. |

Values are case-insensitive at configure time (`STDIO`, `Spdlog`, and `none` normalize to lowercase). Any other value stops configuration with `message(FATAL_ERROR)` and lists the valid values.

Logging headers are grouped under `<tpmkit/logging/...>`; see [ADR-004](.compozy/tasks/stdio-logger-adapter/adrs/adr-004.md).

- `<tpmkit/logging/logger.h>`
- `<tpmkit/logging/noop_logger.h>`
- `<tpmkit/logging/spdlog_api.h>`
- `<tpmkit/logging/spdlog_logger.h>`
- `<tpmkit/logging/stdio_api.h>`
- `<tpmkit/logging/stdio_logger.h>`

The test recording adapter keeps its test-helper path: `<tpmkit/testing/recording_logger.h>`.

The `stdio_logger` adapter writes one structured record per line. `error` and `warn` records go to stderr; `info`, `debug`, and `trace` records go to stdout. Each line contains a UTC timestamp with millisecond precision, a bracketed level token such as `[INFO ]`, the message, and structured fields rendered as `key=value`. The adapter flushes after every record.

Color is controlled by `tpmkit::color_mode`: `auto_`, `always`, or `never`. In `auto_` mode, color is emitted only for TTY streams; a non-empty `NO_COLOR` disables color, and a non-empty `FORCE_COLOR` enables color only when `NO_COLOR` is unset. The `always` and `never` modes are explicit overrides and ignore both environment variables. Tests and embedding tools can pass borrowed output streams through `stdio_logger_options::out` and `stdio_logger_options::err`.

At the composition root:

```cpp
#include <tpmkit/logging/stdio_logger.h>

#include <memory>

auto log = std::make_shared<tpmkit::stdio_logger>();
config.log = log;
```

### Migration

Builds that previously enabled the spdlog adapter with the former boolean now use the enum value `-DTPMKIT_LOG_ADAPTER=spdlog`. Include logging headers from `<tpmkit/logging/...>`, and select `-DTPMKIT_LOG_ADAPTER=stdio` when a zero-dependency stdout/stderr adapter is preferred.

## Test Substitutes (`tpmkit::testing::`)

This namespace is experimental until the first downstream module stabilizes its unit-test surface. It is built as a separate target so production consumers do not link test doubles by default; see [ADR-003](.compozy/tasks/esys-context/adrs/adr-003.md).

## Supported Toolchains

- GCC >= 9
- Clang >= 10
- MSVC >= 19.28 (Visual Studio 2019 16.8)

## Documentation

Public headers carry Doxygen comments. Generate the API reference with:

```sh
./scripts/exec-tpmkit-docker.sh 'cmake -S . -B build/docs -DTPMKIT_BUILD_DOCS=ON'
./scripts/exec-tpmkit-docker.sh 'cmake --build build/docs --target docs'
```

The Doxygen configuration template lives at [docs/Doxyfile.in](docs/Doxyfile.in).

## License

tpmkit is distributed under the MIT License. See [LICENSE](LICENSE).
