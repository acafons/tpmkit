# tpmkit
This is a C++17 library that wraps TPM2 TSS stack

Test substitutes under `tpmkit::testing::` are experimental until the first
downstream module stabilizes its unit-test surface.

## Build presets

Use the CMake presets inside the dev container. Presets keep generated files
under `build/<preset>`:

- `debug` -> `build/debug`
- `release` -> `build/release`
- `asan` -> `build/asan`
- `ubsan` -> `build/ubsan`
- `tsan` -> `build/tsan`
- `coverage` -> `build/coverage`

For example:

```sh
./scripts/run-tpmkit-docker.sh
./scripts/exec-tpmkit-docker.sh 'cmake --preset debug'
./scripts/exec-tpmkit-docker.sh 'cmake --build --preset debug'
./scripts/exec-tpmkit-docker.sh 'ctest --preset debug'
```
