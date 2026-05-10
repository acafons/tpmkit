# tpmkit project instructions

This is a C++17 library that wraps TPM2 TSS stack (FAPI/ESYS) behind a hexagonal architecture. The full set of always-on rules lives under `.claude/rules/` and is imported below.

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
