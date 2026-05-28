---
name: tpm-commit-pr
description: Commit and pull-request rules for the tpmkit project — Conventional Commits format, branch naming, PR description requirements (motivation, test evidence, security impact), required CI gates, and squash-merge policy. Use when writing a commit message, opening a PR, or reviewing PR hygiene. Do not use for source code changes, build configuration, or test writing.
---

# Git and Pull Requests — tpmkit

## Procedure

Apply when creating a commit:

1. **Check for staged changes.** Run `git diff --cached --name-status`. If nothing is staged, stop and ask the user which files to stage. Never run `git add -A` or `git add .` to "fix" an empty stage.
2. **Draft the message** per the rules in "Commits" below.
3. **Present the draft for approval before invoking `git commit`.** Show it as labeled lines rather than a fenced code block — fenced blocks render blank in some UIs:

   **Title:** `feat(crypto): add RSA-PSS verifier`

   **Body:**
   `Implement TPM2_VerifySignature wrapper for RSA-PSS schemes`
   `to support extended key usage scenarios.`

   **Trailers:** `Refs: #123`

4. **Wrap the body at 72 characters before invoking `git commit`** — `git` does not auto-wrap. Pass the whole message through a single `-m` with a HEREDOC whose lines are already wrapped, so line breaks land verbatim:

   ```sh
   git commit -m "$(cat <<'EOF'
   feat(scope): subject under 50 chars

   Body paragraph wrapped at 72 characters. Successive lines of the
   same paragraph stay on consecutive lines; a blank line separates
   one paragraph from the next.
   EOF
   )"
   ```

   **Do not pass each wrapped line as its own `-m`.** `git` joins multiple `-m` values with a *blank line* between them — one `-m` is one paragraph, not one line. Splitting a single wrapped paragraph across several `-m` flags inserts blank lines mid-paragraph and breaks the prose. Use one `-m` per intended paragraph only (e.g. a separate `-m` for a `BREAKING CHANGE:` footer or a `Refs: #n` trailer), never one per visual line.

### Autonomous mode

When the commit happens unattended (headless SDK agent, scheduled `/loop`, CI bot, post-codegen workflow), the interactive guards in steps 1 and 3 relax — but not by skipping:

- **Staging.** Stage **only the files the agent just produced or edited, by explicit path.** The `git add -A` / `git add .` ban still holds; an unsupervised accidental commit of a `.env`, a build artifact, or someone else's working-tree change is the *same* accident as a supervised one — worse, because no one is watching.
- **Approval.** The drafted message is final; there is no interactive revision step. The PR review is the gate. The agent still produces the labeled draft in its run log so the operator can audit after the fact.
- **What does not relax.** Conventional Commits, the ≤ 50-character header, the 72-character body wrap, single-word scopes, one-logical-change-per-commit, and the entire "Forbidden" section apply identically.
- **Authorization.** The agent does not decide on its own that commits are allowed. The system rule "only commit when explicitly asked" is satisfied by the operator who configured the workflow (launch flag, workflow YAML, agent prompt), not by the agent inferring intent. If that authorization signal is absent, the agent stops at step 3 and surfaces the drafted message instead of committing.

### Signed commits

Pass `-S` (or `--gpg-sign`) to `git commit` when any of the following holds:

- The user explicitly asks for it ("sign the commit", "GPG-sign", "SSH-sign", "signed commit").
- Repository or user config enables it: `git config --get commit.gpgsign` returns `true`.
- The branch's existing commits are signed (`git log --show-signature -1` shows a valid signature) — match the convention.

Mechanics and discipline:

- Use the signing key configured in `user.signingkey`. Do not override it from the command line.
- Never pass `--no-gpg-sign` or `-c commit.gpgsign=false` to work around a signing failure. If signing fails (missing key, expired key, agent not running, passphrase prompt in a headless context), stop and surface the underlying error — do not fall back to an unsigned commit.
- Signing is additive. Every other rule in this skill still applies: Conventional Commits, the ≤ 50-character header, the 72-character body wrap, the staging guard, the draft-and-approve loop.
- **Autonomous mode.** When the workflow requires signed commits, a usable signing key must already be available (key present, agent running, no interactive passphrase). If the key is missing or signing fails, abort and surface the failure — do not produce an unsigned commit to "make progress."
- **Verification.** After committing, `git log --show-signature -1` confirms the signature applied. Include the confirmation in the run log when operating autonomously.

## Commits

- Conventional Commits: `type(scope): subject`. Types: `feat`, `fix`, `refactor`, `test`, `docs`, `build`, `ci`, `perf`, `security`, `chore`.
- Scopes are **single words**, derived from the architecture layout. Examples: `domain`, `crypto`, `keys`, `fapi`, `esys`, `openssl`, `mock`, `composition`, `build`, `ci`, `docs`. No nested or path-style scopes (`adapters/tpm2_fapi`) — they eat the title budget.
- Subject line: imperative mood, **≤ 50 characters** (full header including `type(scope):` prefix), no trailing period. The 50-char cap matches `git log --oneline` and PR-list rendering.
- **Breaking changes** append `!` to the type/scope: `feat(crypto)!: remove deprecated digest API`. Include a `BREAKING CHANGE: <rationale>` footer in the body. The `!` is the SemVer major-bump signal — `.claude/rules/library-api-design.md` (Versioning and deprecation) treats every `!` commit as a major bump trigger. Omitting it on a real break silently consumes the signal.
- Body explains *why*, not *what*. Reference issues by number when relevant.
- Security fixes use the `security:` type and reference the CVE in the body if one has been issued.
- **Embargoed fixes** do **not** name the vulnerability or attack vector before disclosure. While embargoed, use neutral hardening/refactor language (`refactor(crypto): tighten input validation in <module>`) and keep the branch private. At disclosure time, reword the squash subject to `security: …` and add the CVE reference. The commit history is public the moment it pushes — assume an attacker reads it.
- One logical change per commit. No "misc fixes" commits — split them.
- **Bisectable history.** Every commit that lands on `main` must build green and pass the test suite on its own. `git bisect` is the primary tool for hunting regressions in crypto/TPM code, and a single broken commit in a series breaks bisection across the entire range. If a multi-commit series temporarily breaks the build mid-way, squash it before merge.

## Branching

- `main` is always releasable. No direct commits.
- **Branch prefix matches the Conventional Commits type** of the squash commit (see "Commits") — one vocabulary across branch listings and `git log`.

  | Prefix | Use for |
  |---|---|
  | `feat/<desc>` | New feature or capability |
  | `fix/<desc>` | Bug fix landing on `main` |
  | `refactor/<desc>` | Internal restructuring, no behavior change |
  | `perf/<desc>` | Performance work backed by a benchmark |
  | `test/<desc>` | Test-only changes |
  | `docs/<desc>` | Documentation only |
  | `build/<desc>` | Build system, CMake, or dependency wiring |
  | `ci/<desc>` | CI configuration |
  | `chore/<desc>` | Maintenance: deps bumps, formatting, repo housekeeping |
  | `security/<desc>` | Security fix (see "Commits" for embargo language) |

- `<desc>` is `kebab-case`, ≤ 40 characters, and describes the change — not the ticket number. `feat/rsa-pss-verifier`, not `feat/JIRA-1234`.
- **`hotfix/<desc>` is a workflow signal, not a commit type.** Used for emergency fixes against a shipped version:
  - Branch from the affected release point (release branch or tag, per `tpm-release`).
  - Squash commit subject is `fix:` or `security:` — the `hotfix/` prefix lives on the branch only.
  - Forward-port to `main` after merge (cherry-pick or merge).
  - Pair with a patch-version tag and the expedited review path documented in `tpm-release`.
- Long-lived branches require a documented reason; rebase onto `main` regularly.

## Pull requests

- Every change goes through a PR. No exceptions for "trivial" changes — the value is the CI gate, not the review burden.
- PR description includes: motivation, summary of changes, test evidence (which tests cover the change), security impact (`none` / `changes threat surface` / `fixes vulnerability`).
- A PR that adds a third-party dependency must include the security review described in the `tpm-build-config` skill (Dependency management).
- All CI checks must pass before merge: build, tests, sanitizers, static analysis, formatting, doc coverage.
- Squash-merge to `main`. The squash commit subject follows Conventional Commits.

### PR description template

Every PR description fills in this template. Omit a section only when its leading note says it is omittable.

```markdown
## Summary

<One or two sentences: what changed and why. This becomes the squash commit body.>

## Motivation

<The problem this PR solves. Link the issue: `Refs: #<n>` or `Closes: #<n>`.>

## Changes

- <One bullet per logical change. Mirrors the commit list in the branch.>

## Test evidence

- <Test files / cases that cover the change.>
- <Commands run locally (e.g., `ctest --preset asan`, `ctest --preset ubsan`).>
- <For UI-less changes that can't be exercised by tests, state so explicitly.>

## Security impact

**Classification:** `none` | `changes threat surface` | `fixes vulnerability`

<Required when classification is not `none`. One paragraph on what shifted in the threat model. For `fixes vulnerability`, follow the embargo language rules in "Commits" — do not name the vector here until disclosure.>

## Breaking changes

<Omit when there are none. Otherwise: describe the API/ABI break and the migration path. Must match the `!` marker on the squash commit subject and trigger the SemVer major bump per `.claude/rules/library-api-design.md`.>

## Dependencies

<Omit when no third-party dep is added or bumped. Otherwise: link the security review (license, maintenance status, CVE history) per `tpm-build-config` (Dependency management).>
```

Notes on filling it in:

- **Summary becomes the squash commit body** at merge — write it for `git log`, not for the PR view. No screenshots, no `@mentions`, no trailing emoji.
- **Security classification is required even when `none`.** Forcing the field eliminates the "I forgot to think about it" failure mode.
- **`Closes: #<n>` in Motivation** auto-closes the linked issue on merge; `Refs: #<n>` links without closing. Pick deliberately.
- **CI gates are not part of the description.** Reviewers see them in the PR's checks panel; restating them in prose is noise.

## Forbidden

- Force-push to `main`. Force-push to a shared feature branch requires coordination.
- Bypassing CI (e.g., merging with failing checks) for any reason.
- Committing generated files, build artifacts, secrets, or `.env` files. Use `.gitignore` aggressively.

## Error Handling

* **Nothing staged (interactive).** Stop and ask the user which files to stage. Do not infer the intent from working-tree changes.
* **Nothing staged (autonomous).** Abort the commit and log the empty stage as a workflow error — the agent should have staged its own outputs by explicit path before reaching this step. Do not fall back to `git add -A` / `git add .`.
* **User rejects the draft (interactive).** Revise based on the feedback and present the new draft for approval. Do not invoke `git commit` until an approved draft exists. (Not applicable in autonomous mode — the drafted message is final; PR review is the gate.)
* **Pre-commit hook fails.** Fix the underlying issue, re-stage, and create a *new* commit. Never `--amend` — the hook failure means the commit was not recorded, so amending would alter the previous (unrelated) commit.
* **Signing fails when a signed commit was requested.** Stop. Do not retry with `--no-gpg-sign` or `-c commit.gpgsign=false`. Diagnose the cause: missing `user.signingkey`, expired or revoked key, `gpg-agent`/`ssh-agent` not running, or an interactive passphrase prompt in a headless context. Surface the diagnostic to the user (interactive) or the run log (autonomous), and resume only once signing is fixed.
* **Required CI gate is failing on the PR.** Do not relax or skip the gate. Fix the failure or, if the gate itself is wrong, open a separate PR that justifies the change to the gate (cross-reference: `tpm-build-config` "CI matrix and gates").
* **Secret committed and pushed.** **Rotate the credential first** — history rewriting does not revoke a secret that has reached the remote, since anyone with a clone (or a CI cache, or a fork) retains it. Order: (1) rotate the secret at the issuing system, (2) confirm the old credential no longer authenticates, (3) scrub history with `git filter-repo` and force-push (coordinate with the team per "Forbidden"), (4) notify the security review channel. Skipping step 1 and going straight to history rewriting is the single most common mistake — and the most expensive one.
