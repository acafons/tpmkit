# tpmkit project instructions

This is a C++17 library that wraps TPM2 TSS stack (FAPI/ESYS) behind a hexagonal architecture. The full set of always-on rules lives under `.claude/rules/` and is imported below.

## Context and frequent commands

### Critical rule: container-only builds and commands

**All** builds, tests, and other commands that use the tpmkit toolchain, CMake layout, or TPM stack must be run **inside** the dev container. From the repository root, wrap the full shell command in a single argument to `./scripts/exec-tpmkit-docker.sh` (for example `./scripts/exec-tpmkit-docker.sh 'cmake --build build/debug'` or `./scripts/exec-tpmkit-docker.sh 'ctest --test-dir build/debug'`). Start or ensure the container is running first with `./scripts/run-tpmkit-docker.sh`. Do not run those builds or commands on the host OS (for example bare macOS) unless a task is explicitly host-only (such as editing files with no toolchain invocation).

### Context

Typical local work uses a Linux dev container image (`tpmkit:latest` by default) named `tpmkit-dev`, with the repo’s `.tpm-state/` bind-mounted into the container as `TPMKIT_STATE_DIR` (default `/home/app/.tpm-state`) so simulator files, NVChip, and logs persist on the host. The container image does not start the TPM stack by itself; inside the container, `tpm-start.sh` brings up swtpm (`tpm_server`), D-Bus, and `tpm2-abrmd`, and `tpm-stop.sh` tears them down.

### `./scripts/` (frequent commands)

All paths are from the repository root.

| Script                            | Role                                                                                                                                                                                                                                                          |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `scripts/run-tpmkit-docker.sh`    | Create or start the dev container (`docker run` with `--init`, or `docker start`). Flags: `-d`/`--detach` (default), `-f`/`--foreground`. Env: `TPMKIT_IMAGE`, `TPMKIT_CONTAINER_NAME`, `TPMKIT_HOST_DATA` (host dir for state; default `<repo>/.tpm-state`). |
| `scripts/exec-tpmkit-docker.sh`   | Run a command in the container via `sh -c` (pipes/redirections work when passed as one string). Env: `TPMKIT_CONTAINER_NAME`. Example: `./scripts/exec-tpmkit-docker.sh 'cmake --build build/debug'`.                                                         |
| `scripts/tpmkit-docker-status.sh` | Probe container state for humans/CI/agents. Exit: `0` running, `1` stopped, `2` missing, `3` usage, `4` Docker unreachable. `-q`/`--quiet`.                                                                                                                   |
| `scripts/stop-tpmkit-docker.sh`   | `docker stop` and by default `docker rm`. `--keep` / `-k` stops only. Env: `TPMKIT_CONTAINER_NAME`.                                                                                                                                                           |
| `scripts/tpm-start.sh`            | Intended **inside** the Linux container: start `tpm_server`, D-Bus, `tpm2-abrmd`. Uses `TPMKIT_STATE_DIR` (default `$HOME/.tpm-state` in the image). Not for bare macOS/Windows shells unless `TPMKIT_ALLOW_NONCONTAINER=1`.                                  |
| `scripts/tpm-stop.sh`             | Intended **inside** the same environment as `tpm-start.sh`: stop those daemons. Optional `--clean` deletes NVChip for a full TPM state reset.                                                                                                                 |

Handy sequence: `./scripts/run-tpmkit-docker.sh` → `./scripts/tpmkit-docker-status.sh` → `docker exec -it tpmkit-dev tpm-start.sh` (or `./scripts/exec-tpmkit-docker.sh tpm-start.sh`) before running builds/tests that need a TPM. Stop stack in-container with `tpm-stop.sh`; stop/remove the container with `./scripts/stop-tpmkit-docker.sh`.

Activity-specific guidance is in `.agents/skills/`:

- `tpm-write-code` — when writing or refactoring production source under `src/` or `include/<library>/`
- `tpm-write-tests` — when writing or modifying tests
- `tpm-build-config` — when modifying CMake, CI, or dependencies
- `tpm-write-docs` — when writing Doxygen, README, CHANGELOG, examples, or ADRs
- `tpm-write-logging` — when adding log call sites, implementing a logger adapter, or evolving the event schema
- `tpm-commit-pr` — when writing commit messages or opening PRs
- `tpm-add-port-or-adapter` — when introducing a new port or backend adapter
- `tpm-debug` — when triaging a failing test, an unexpected error, a hang, or a crash
- `tpm-security-review` — when reviewing a security-sensitive PR or running a pre-release audit
- `tpm-release` — when cutting a release, bumping the library version, or finalizing a release branch

## Always-on rules

@.claude/rules/code-standards.md
@.claude/rules/architecture.md
@.claude/rules/library-api-design.md
@.claude/rules/security.md
@.claude/rules/error-handling.md
@.claude/rules/concurrency.md
@.claude/rules/performance.md
@.claude/rules/logging.md
