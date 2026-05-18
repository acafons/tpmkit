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
```

The source for that program is [examples/tpm_context_basics.cpp](examples/tpm_context_basics.cpp).

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

Dependencies are pinned in [vcpkg.json](vcpkg.json): `tl-expected` 1.3.1, `ms-gsl` 4.2.1, `tpm2-tss` 4.1.3, `spdlog` for the optional logger adapter, and `gtest` 1.17.0 port 2. Local project commands that use CMake, the TPM stack, or the toolchain must run through the dev container.

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

Production installs export `tpmkit::tpmkit`. Test substitute headers and the `tpmkit::tpmkit_testing` target are installed only when `TPMKIT_INSTALL_TESTING=ON`; see [ADR-008](.compozy/tasks/esys-context/adrs/adr-008.md). The spdlog adapter is controlled by `TPMKIT_WITH_SPDLOG` and is enabled by default; when disabled, `tpmkit/spdlog_api.h`, `tpmkit/spdlog_logger.h`, and `tpmkit::tpmkit_spdlog` are not installed. Downstream consumers that need it should request `find_package(tpmkit CONFIG REQUIRED COMPONENTS spdlog)` and link `tpmkit::tpmkit_spdlog`.

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
