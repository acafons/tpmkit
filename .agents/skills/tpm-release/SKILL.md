---
name: tpm-release
description: Release procedure for the tpmkit C++17 library — version bump rules from Conventional Commits, CHANGELOG promotion (Unreleased section to a versioned section), git tag creation, ABI compatibility check, dependency CVE sweep, and post-release branch hygiene. Use when cutting a release, bumping the library version, finalizing a release branch, or preparing release notes. Do not use for routine commits, individual PR review, or feature work that is not yet release-ready.
---

# Release — tpmkit

This skill is the procedure for cutting a tpmkit release. The version-bump policy comes from `library-api-design.md` (Versioning and deprecation), the commit-format inputs come from `tpm-commit-pr`, and the CHANGELOG format comes from `tpm-write-docs`. This skill chains them.

A release is a cross-cutting event — it touches code, docs, tags, dependencies, and the threat model. Skipping a step is how a "trivial" patch release ships an ABI break or a stale CHANGELOG. Walk the procedure end to end every time.

## Pre-flight

Before doing anything else:

1. **Confirm `main` is releasable.** `main` is always releasable per `tpm-commit-pr` Branching, but verify: every CI gate green on the most recent commit, no open security advisories on dependencies, no PRs in flight that would have wanted to land in this release.
2. **Pick the version.** Compute the version from the commits since the previous tag — see *Version-bump rules* below. Do not pick a version by intuition; the rules are deterministic.
3. **Decide the release type.** Patch (no behavior change), minor (additive), or major (breaking). The Conventional Commits log determines this; the commit log is the source of truth.

## Version-bump rules

Cross-reference: `library-api-design.md` (Versioning and deprecation), `tpm-commit-pr` (Conventional Commits, breaking-change marker).

Compute the bump from the commit types between the previous tag and HEAD:

| Commit pattern between previous tag and HEAD | Bump |
|---|---|
| At least one `!` after type/scope, or a `BREAKING CHANGE:` footer | **Major** |
| At least one `feat(...)` (and no breaking changes) | **Minor** |
| Only `fix(...)`, `refactor(...)`, `perf(...)`, `security:`, `docs(...)`, `test(...)`, `build(...)`, `ci(...)`, `chore(...)` | **Patch** |

Mechanical extraction:

```bash
git log <previous-tag>..HEAD --pretty=format:'%s'
```

Read the output, classify each line, take the highest tier that appears. A single `!` in the entire range mandates a major bump — there is no "small breaking change" exception.

**Pre-1.0 versions.** Until the first 1.0 release, breaking changes bump the *minor* version (0.X.0) and feature additions bump the *patch* (0.X.Y). The major version stays at 0. State the pre-1.0 mode at the top of the CHANGELOG.

**Pre-release suffixes.** Use `-alpha.N`, `-beta.N`, `-rc.N` suffixes during stabilization. The numeric portion is monotonic; do not reset it on a version bump within the same line.

## Procedure

### 1. Gather the changes

```bash
git fetch --tags
PREV=$(git describe --tags --abbrev=0)
git log "$PREV"..HEAD --pretty=format:'%h %s'
```

Save this as the input to the CHANGELOG and release notes. Do not paraphrase the commit subjects — they were written for `git log`, which is what release-note readers will run later (`tpm-commit-pr` PR description template).

### 2. Determine the version

Apply the version-bump rules to the commit list. Write the chosen version. If the result is contested ("does this commit count as a breaking change?"), the answer is in the commit message — if it has a `!`, it is breaking; if it does not, it is not. Do not retroactively reinterpret commits.

### 3. Run the security sweep

Cross-reference: `tpm-security-review`, `tpm-write-tests` Periodic offline audit, `security.md` Dependencies and CVE tracking.

- Check OpenSSL and TPM2 TSS upstream advisories since the previous release. Bumping a dependency for a security fix takes priority over feature work — if a fix is available, include it in this release.
- Check vcpkg manifest pins. Each pinned dependency: any CVE published since the pin? If yes, decide whether to bump now or document the deferred bump.
- Run `tpm-security-review` against the diff between the previous tag and HEAD if any commit in the range has `security:` type or non-`none` security impact.
- **Run the periodic offline audit** per `tpm-write-tests` Periodic offline audit: Valgrind Memcheck sweep, full NIST SP 800-22 + dieharder RNG suite, extended (≥8h) fuzz campaign, and multi-hour stress runs. Each is a release blocker on failure; results land under `audit/<YYYY-MM-DD>/` committed alongside the release tag commit.
- If a CVE-fixing dependency bump is included, the CHANGELOG entry references the CVE.
- If a Valgrind suppression was added or modified during the audit, the PR carries the reviewer line per the suppression-file discipline in `tpm-write-tests` — and any suppression touching entropy/RNG/uninitialized-memory in crypto code is treated as a critical-path review item, not a routine bump.

### 4. Run the ABI check

For non-major bumps, ABI must not break. Run the ABI-compatibility check tool the project uses (`abidiff` against the previous installed library — see `tpm-build-config` for the tool the project standardizes on). A non-empty ABI diff on a minor or patch release means one of:

- A commit was missing the `!` marker; promote to a major bump and update the CHANGELOG.
- The change is not actually ABI-affecting and the tool is reporting a false positive; document the verdict in the release notes.
- The change is ABI-affecting but unintentional; revert before tagging.

For major bumps, the ABI diff is informational — record it in the release notes so consumers can see what changed.

### 5. Promote the CHANGELOG

Cross-reference: `tpm-write-docs` (CHANGELOG format).

The project follows Keep a Changelog. Promote the `Unreleased` section to a versioned section:

```markdown
## [Unreleased]

(empty — refill as new commits land)

## [X.Y.Z] - YYYY-MM-DD

### Added
- ...

### Changed
- ...

### Deprecated
- ...

### Removed
- ...

### Fixed
- ...

### Security
- ...
```

Rules for the promotion:

- Use the categories Keep a Changelog defines. Do not invent new ones.
- Each entry is one line; reference issue/PR numbers but not commit hashes (the tag links the hashes).
- `### Security` always appears last and is mandatory if any commit in the range was `security:` type. State the CVE when one has been issued.
- Update the link references at the bottom of the file: `[X.Y.Z]: <repo>/compare/<previous-tag>...vX.Y.Z` and `[Unreleased]: <repo>/compare/vX.Y.Z...HEAD`.
- The `Unreleased` section persists at the top, empty, ready for the next release.

### 6. Update the version metadata

Bump the version in:

- `CMakeLists.txt` (`project(tpmkit VERSION X.Y.Z ...)`).
- The CMake package config that ships with the library (`tpmkitConfigVersion.cmake` is generated from the project version — verify the generation pulls from the project line).
- Any other file that hard-codes the version (avoid these where possible — generate from CMake instead).
- README's compatibility table, if the new version raises a minimum compiler/dep version.

The version-bump commit is its own commit, with subject `chore(release): X.Y.Z`. It contains the CHANGELOG promotion and the version metadata bump and nothing else.

### 7. Open the release PR

Use `tpm-commit-pr`'s PR description template. Title: `chore(release): X.Y.Z`. The Summary section is the release notes — typically the bullet list from the new CHANGELOG section, condensed.

The release PR is reviewed like any other PR. CI must pass; the same gates apply (build, tests, sanitizers, static analysis, formatting, doc coverage). On approval, squash-merge.

### 8. Tag the release

After the squash-merge lands on `main`:

```bash
git fetch origin main
git tag -a vX.Y.Z -m "Release X.Y.Z" <commit-sha>
git push origin vX.Y.Z
```

The tag is annotated (`-a`), not lightweight. The tag message is "Release X.Y.Z" — the detailed notes are in the CHANGELOG, which is what consumers read.

The tag points to the squashed release commit on `main`. Never tag a feature branch.

### 9. Publish artifacts and notes

Whatever the project publishes (source archive, binary packages, GitHub Release):

- The release notes are the new CHANGELOG section, verbatim. Do not rewrite them for the release page — the CHANGELOG is the canonical source.
- Source archive is generated from the tag, not from `main`.
- For a security release, follow the embargo language rules in `tpm-commit-pr` (Commits) — the squash subject and CHANGELOG entry use neutral language until disclosure, then are reworded with the CVE reference.

### 10. Post-release hygiene

- The `Unreleased` section in the CHANGELOG is empty — confirm.
- If a release branch was used (long-lived release branch for major versions), merge it back or rebase it forward per the project's branch strategy.
- Open follow-up issues for any deferred dependency bumps or known-issues called out in the release notes.
- Notify the security review channel (`tpm-commit-pr` mentions this for embargoed fixes; do it for every release with a `### Security` entry).

## Patch and security releases

A patch release is the same procedure with a smaller scope. The two differences:

- The commit range is shorter, the CHANGELOG entry is small.
- Security releases are *time-sensitive*. The ABI check, dependency sweep, and CHANGELOG steps still happen; the security sweep happens *first* so the release notes are accurate.

For an embargoed security release, the embargo language in `tpm-commit-pr` (Commits) applies to:

- Commit subjects on the branch (use neutral language).
- The CHANGELOG entry on the release commit (reword to the security-disclosure form at disclosure time, in a follow-up commit).
- The PR description (do not name the vector before disclosure).
- The release tag's annotation (use the disclosure-time wording).

After disclosure, file a CHANGELOG amendment commit that reworps the relevant entry to its final form. The original commit that merged the release stays as is — git history is public.

## Common mistakes

- **Bumping the version by intuition** instead of by the commit log. The version-bump rules are mechanical for a reason.
- **Skipping the ABI check on a "trivial" change.** ABI breaks ship as a minor release this way more often than anyone admits.
- **Forgetting to refill the `Unreleased` section.** The next release reads "no changes" until someone notices.
- **Tagging from a feature branch.** The tag must point at a commit on `main`.
- **Using a lightweight tag (`git tag X.Y.Z`).** Annotated tags carry metadata that consumers and tools rely on.
- **Letting `chore(release): X.Y.Z` carry unrelated cleanup.** It must be the version bump and the CHANGELOG promotion only.

## Error Handling

* **Version-bump rule conflicts with intuition.** The rule wins. A single `!` in the commit range mandates a major bump — there is no "small breaking change" exception. If a commit was mistakenly marked breaking, fix the commit message in the next branch, not the release version.
* **ABI diff fails on a non-major bump (Step 4).** Decide deliberately: (1) promote the bump to major and update the CHANGELOG, (2) confirm the tool reports a false positive and document the verdict in the release notes, or (3) revert the ABI-affecting change before tagging. Do not ship a non-major release that breaks ABI.
* **Dependency CVE found during the security sweep (Step 3).** Bumping for the fix takes priority over feature work (`security.md` Dependencies and CVE tracking). Either include the bump in this release with a `### Security` entry in the CHANGELOG, or document the deferred bump with a follow-up issue and a reason it cannot land now.
* **Tag would point to a feature branch.** Wrong. Tags only point to the squashed release commit on `main`. Wait for the release PR to merge, then tag the commit on `main`.
* **Lightweight tag (`git tag X.Y.Z`) was created instead of annotated.** Delete it and recreate with `-a`. Annotated tags carry metadata that consumers and tools rely on; lightweight tags are silently incompatible.
* **`chore(release): X.Y.Z` commit carries unrelated cleanup.** Split it. The release commit contains the version metadata bump and the CHANGELOG promotion only — anything else makes `git bisect` and the release-notes audit trail noisier than they need to be.
* **`Unreleased` section in the CHANGELOG is non-empty after Step 5.** The promotion was incomplete. Re-run the promotion: move every entry under the new `[X.Y.Z]` section and leave `[Unreleased]` empty.
* **Embargoed security release: language slipped into a public-facing artifact.** Cross-reference `tpm-commit-pr` Commits (embargo language). Reword the commit subject, CHANGELOG entry, PR description, and tag annotation to neutral language *before* push. After disclosure, file a CHANGELOG amendment commit with the final wording; the original release commit history stays as is.

## Cross-references

- `library-api-design.md` — SemVer, deprecation cycle.
- `tpm-commit-pr` — Conventional Commits, `!` breaking marker, embargo language, PR template.
- `tpm-write-docs` — CHANGELOG format (Keep a Changelog), README structure.
- `tpm-security-review` — pre-release security audit checklist.
- `tpm-build-config` — vcpkg pins, CMake version metadata, ABI tooling.
- `security.md` — dependency CVE tracking, build hardening (release builds).
