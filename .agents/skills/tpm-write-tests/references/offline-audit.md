# Periodic Offline Audit

Read this reference when preparing a release, running a quarterly audit,
executing Valgrind/RNG/fuzz/stress extended checks, or interpreting audit
failures. These checks cost too much to run per PR but matter enough to run
before every release. They are scheduled work, not part of the CI matrix.

Audit results live in `audit/<YYYY-MM-DD>/` under the repo for the same
audit-trail discipline as a security review. Cross-reference `tpm-release` Step
3 (security sweep); the release procedure pulls from this reference.

## Valgrind Memcheck Sweep

**Cadence:** every release tag, plus quarterly between releases.

**Purpose:** catch uninitialized-memory reads that ASan does not, and exercise
the release binary rather than an instrumented build. Complementary to ASan, not
a replacement. Valgrind runs roughly 10-50x slower, so it cannot live in the
per-PR matrix. Run on Linux only; Valgrind has no working macOS support and no
Windows support.

**Procedure:**

```bash
cmake --preset release
cmake --build --preset release
valgrind --tool=memcheck \
  --leak-check=full --show-leak-kinds=all \
  --track-origins=yes --error-exitcode=1 \
  --suppressions=audit/valgrind/tpmkit.supp \
  --gen-suppressions=all \
  ./build/release/tests/<test-binary>
```

Run against the unit, integration, and contract test binaries plus each stress
harness. Capture stdout and stderr to `audit/<date>/valgrind-<test>.log`. The
`--gen-suppressions=all` output goes into a scratch file for review; never copy
it into `tpmkit.supp` without the discipline below.

**Suppression-file discipline.** `audit/valgrind/tpmkit.supp` is a
security-relevant artifact and is reviewed like a dependency manifest:

- Every suppression names the upstream component, such as `openssl-3.2`,
  `tss2-fapi-4.0`, or `glibc-2.38`; the version it was added against; a
  one-line reason the warning is benign; and the reviewer who approved it.
- A new suppression follows the same review path as adding a third-party
  dependency (`tpm-build-config` Dependency management). Record the reviewer's
  name in the entry.
- After every dependency bump, re-validate every suppression against that
  dependency. Run Valgrind unsuppressed and confirm the warning still appears in
  the new version's code paths. Delete stale suppressions in the same PR as the
  bump.
- Entropy, RNG, and uninitialized-memory warnings in cryptographic code are
  never suppressed without an upstream vendor reference explaining why the read
  is intentional. This rule is the lesson of CVE-2008-0166. No vendor reference,
  no suppression; when in doubt, file with upstream and wait.

**Reading the output.** A clean run produces no error records and a leak summary
with zero "definitely lost" and zero "possibly lost". "Still reachable" is
informational on a short-lived test binary; on a long-running stress harness it
is a signal worth investigating.

## Full RNG Statistical Suite

**Cadence:** every release tag.

**Purpose:** the unit-tier RNG sanity test catches constant-output regressions
in 1 MiB. The full statistical battery catches subtle bias, such as a
working-but-degraded RNG that passes the unit test but produces detectably
non-uniform output at scale.

**Procedure:** generate at least 1 GiB from each documented RNG source
(`RAND_bytes`, `Esys_GetRandom`, host RNG) into
`audit/<date>/rng/<source>.bin`. Run NIST SP 800-22 and dieharder against each
file.

**Reading the output.** Statistical tests are statistical; an isolated low
p-value over many sub-tests is expected. The threshold is the documented
multi-test pass criterion in the SP 800-22 reference. Do not invent ad-hoc
thresholds. A genuine suite-level failure is a release blocker. Capture the
failing sample and the seed before filing.

## Extended Fuzz Campaign

**Cadence:** every release tag.

**Purpose:** the per-PR fuzz job runs each harness for roughly 10 minutes. An
overnight campaign explores deeper input space and surfaces inputs the short job
cannot reach.

**Procedure:** run each harness in `tests/fuzz/` for at least 8 hours under ASan
and UBSan. New crashes pin in `tests/fuzz/corpus/<harness>/regressions/` per the
fuzz-tier rule. The release does not ship until each new crash is fixed or has a
documented deferral with a security-impact review.

## Stress Harness Full-Cadence Run

**Cadence:** every release tag.

**Purpose:** the nightly stress job runs each harness for 10-30 minutes. The
release run extends each to its documented multi-hour ceiling, catching
slow-growth leaks, fragmentation patterns, and DA-lockout edge cases that only
appear at scale.

**Procedure:** run `TPMKIT_STRESS_DURATION=8h ctest --preset tsan-stress
--label-regex Stress`. Capture per-harness wall-clock and resident-set growth to
`audit/<date>/stress-<harness>.csv`. Unbounded growth in either dimension is a
release blocker.

## Audit-Result Lifecycle

- Commit audit results as plain text under `audit/<YYYY-MM-DD>/` alongside the
  release tag commit, not as a separate branch or out-of-tree store.
- Treat a failed audit as a release blocker. There is no quarantine tier; fix
  the cause or defer the release.
- Keep audit results from previous releases indefinitely for trend analysis. Do
  not prune. A quarterly between-release audit lands in the same
  `audit/<date>/` shape, with a one-line note naming it as a between-release
  audit.
- Reference the latest audit directory by date from the release-time security
  sweep (`tpm-release` Step 3) so reviewers can compare release to release.
