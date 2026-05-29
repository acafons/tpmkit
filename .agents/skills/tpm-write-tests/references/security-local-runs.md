# Running Security Tests Locally

Read this reference when reproducing security-test failures, gating a
security-relevant change before push, iterating on a new security test, or
deciding which preset/label filter to run locally.

Run tpmkit toolchain commands inside the dev container, following the repository
AGENTS.md container-only rule. From the repository root, wrap each command in
`./scripts/exec-tpmkit-docker.sh '<command>'` unless already inside the
container.

## Sanitizer-Based Tests

Configure once, run as many times as needed. The presets bake in
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1` and the matching
TSan/UBSan options from `tpm-build-config`.

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure
```

The same shape works for `ubsan` and `tsan`. ASan and TSan cannot link together;
pick one preset per build.

## Security Labels

Each security test carries the `Security` CTest label, plus a sub-label
(`Mlock`, `Zeroization`, `Oracle`, `Allowlist`, `RngSanity`, `SizeValidation`,
`SessionEnc`, `TpmAuth`). Filter to a subset:

```bash
ctest --preset asan --label-regex Security --output-on-failure
ctest --preset asan --label-regex 'Zeroization|Oracle' --output-on-failure
ctest --preset asan --tests-regex secret_buffer --verbose
```

## Zeroization Optimization Level

Run the secret-zeroization test at `-O2` or higher. The Debug preset's `-O0`
hides the dead-store-elimination class of bug the test exists to catch. Use the
`asan-release` preset (`-O2` with ASan instrumentation) when iterating on a
zeroization test that passes under `asan` but is suspected to drift under
optimization:

```bash
cmake --preset asan-release
cmake --build --preset asan-release
ctest --preset asan-release --tests-regex zeroization --output-on-failure
```

## Integration Security Tests

TPM authorization-failure and session-encryption tests require swtpm running.
Start it in another terminal; the socket paths are the same ones the test
fixtures expect:

```bash
swtpm socket --tpm2 \
  --server type=unixio,path=/tmp/tpmkit-swtpm-socket \
  --ctrl type=unixio,path=/tmp/tpmkit-swtpm-ctrl \
  --tpmstate dir=/tmp/tpmkit-swtpm-state \
  --flags startup-clear
```

Then run the integration tier:

```bash
ctest --preset asan --label-regex 'Integration_tpm2_esys|TpmAuth|SessionEnc' --output-on-failure
```

The integration fixtures assert the local swtpm `--version` matches the pinned
major.minor before running, and skip with a clear message on mismatch. Local
version drift should not produce confusing failures.

## Stress Tier

Stress harnesses are nightly in CI and manual locally. They are not part of
`ctest --preset asan` by default because they take too long. Run them under the
dedicated stress preset and budget:

```bash
cmake --preset tsan-stress
cmake --build --preset tsan-stress
TPMKIT_STRESS_DURATION=10m ctest --preset tsan-stress --label-regex Stress --output-on-failure
```

Use a shorter `TPMKIT_STRESS_DURATION`, such as `1m`, while iterating. Restore
the default before pushing. The CI nightly uses the documented per-harness
budget.

## Interop Tier

Interop tests auto-skip when their tool is not on `$PATH`. Confirm what is
available:

```bash
openssl version    # required for Interop_openssl
tpm2 --version     # required for Interop_tpm2_tools
```

Run with:

```bash
ctest --preset asan --label-regex Interop --output-on-failure
```

A `[ SKIPPED ]` line is the expected outcome on a host without the matching
tool; that is not a failure. To actually exercise an interop test, install the
tool at the pinned version from the CI container images.

## Pre-Push Gate

The minimum local check before pushing a security-relevant change is:

```bash
cmake --preset asan && cmake --build --preset asan && \
  ctest --preset asan --label-regex 'Security|Contract' --output-on-failure
```

This runs every `Security`-labelled test plus the full Contract tier, including
the secret-leak sweep and oracle-uniformity test, under ASan. It catches the
failure modes most likely to bite in CI without paying for the full sanitizer
matrix. Run UBSan and TSan separately when the change touches arithmetic on
caller sizes (UBSan) or anything threaded (TSan).
