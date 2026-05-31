---
name: tpm-write-docs
description: Documentation standards for the tpmkit C++17 library — Doxygen requirements for the public API (parameters, errors, exception safety, thread safety), README structure, CHANGELOG format (Keep a Changelog), examples folder, and ADR format. Use when writing or updating Doxygen comments on public API, the README, CHANGELOG, examples, or ADRs. Do not use for inline code comments, source code, or commit messages.
---

# Documentation — tpmkit

## Procedure

Apply this skill in order on every documentation change:

1. **Classify the target.** Identify which surface the change touches:
   * **Public API** — anything in `include/<library>/` (Doxygen on declarations).
   * **Internal** — anything in `src/` (adapter-internal Doxygen, slim form).
   * **Project-level** — README, CHANGELOG, examples, ADRs.
   The remaining steps depend on the classification.
2. **Pick the matching template.** For public API or internal Doxygen, use the templates in "Doxygen templates" below. For CHANGELOG, follow the Keep a Changelog categories in "Project-level documentation". For ADRs, use the title/context/decision/consequences shape.
3. **Apply the tag vocabulary.** Use only the tags listed in "Tag vocabulary" below; reach for a `@note` block instead of an italicised "Note:" line; do not introduce `@author`/`@date`/`@version`.
4. **Cover every required element** per "Public API":
   * Every parameter with purpose, valid range, ownership.
   * Return value with meaning, ownership, lifetime.
   * Errors — which domain error categories can be produced.
   * Exception-safety guarantee — `noexcept` / strong / basic.
   * Thread-safety contract — thread-compatible / thread-safe / single-threaded.
5. **Mark lifecycle** with `@since vX.Y` for new public APIs and pair `[[deprecated]]` with `@deprecated` carrying the migration message and removal version.
6. **Verify CI gates.** The Doxygen-coverage check fails the build on any undocumented public symbol. CI also builds the extracted code samples — broken samples block the PR.

## Public API

- Doxygen comments live with the **declaration**. For anything declared in a header, the comment goes in the header. For things defined only in a `.cpp` (file-local helpers, functions in an anonymous namespace), the comment goes in the `.cpp` next to the definition. Consumers should be able to read the contract without opening the implementation file.
- Public headers begin with an `@file` Doxygen block — `@file <relative/path/file.h>`, a one-line `@brief`, and a short summary when the file groups several related types. Without it, generated per-file pages are bare filenames and consumers lose the entry point into the documentation.
- Public component families document the nested namespace and include folder
  together. A header under `include/tpmkit/pcr/` documents symbols as
  `tpmkit::pcr::*`; future `nv` and `key` headers should document
  `tpmkit::nv::*` and `tpmkit::key::*`. Examples and `@see` links use the
  nested names directly, with no root-level compatibility aliases unless a
  compatibility plan explicitly added them.
- Explicit low-level backend headers, such as `include/tpmkit/tpm2_esys/...`,
  must say they are backend-specific and must point backend-neutral callers to
  the neutral API. Backend-neutral header documentation must not mention raw
  TSS/OpenSSL handle ownership as an available path.
- Every public class, function, type alias, and enum has a Doxygen comment. For public enums, document the enumeration *and* every enumerator — the values *are* the contract (`algorithm_id::ecdsa_p256`, `error::category::security_failure`, `tpm_startup_mode::clear` each carry meaning a consumer cannot infer from the enum's name alone).
- Every public function documents:
  - Each parameter — purpose, valid range, ownership.
  - The return value — meaning, ownership, lifetime.
  - Errors — which domain error categories the function can produce (`input_error`, `security_failure`, etc.). See `.claude/rules/error-handling.md`.
  - Exception safety guarantee — `noexcept`, strong, basic. See `.claude/rules/library-api-design.md`.
  - Thread-safety contract — thread-compatible, thread-safe, single-threaded. See `.claude/rules/library-api-design.md`.
- When a function has multiple public overloads, document the most general overload fully and use `@copydoc <full_signature>` on the others, adding only the lines that differ (which parameter is omitted, what default applies). This avoids drift between near-identical comment blocks. If overloads genuinely diverge in behavior, document the *delta* — not the whole contract again.
- Mark new public APIs with `@since vX.Y` matching the SemVer release that introduced them (`library-api-design.md`). When deprecating, pair the `[[deprecated("use X instead")]]` attribute with an `@deprecated` tag carrying the same migration message and the version of removal — for example, `@deprecated Since v2.0; use sign_v2() instead. Removed in v3.0.`
- Undocumented public API is a CI failure: a Doxygen-coverage check runs on every PR.

## Tag vocabulary

The project uses the Doxygen tags below. Reach for them by default; do not introduce others without a documented reason.

- **Description:** `@brief`, `@file`.
- **Parameters and return:** `@param`, `@param[in]`, `@param[out]`, `@param[in,out]`, `@return`, `@throws`.
- **Behavior callouts:** `@note`, `@warning`.
- **Cross-references:** `@see`.
- **Lifecycle:** `@since`, `@deprecated`.
- **Code samples:** `@code`, `@endcode`.

Do not use `@author`, `@date`, `@version`, or `@copyright` in source — authorship and revision history live in git, and copyright belongs at the file or project level (LICENSE, top-of-file SPDX line). Prefer Doxygen tags over Markdown for structured information: a `@note` block, not an italicised "Note:" line — tag-based markup renders predictably across themes.

## Doxygen templates

Use the templates below as the *shape* of a Doxygen block. Public types include a usage example and a `@see` chain so consumers can navigate from one type to its collaborators; internal types stay slim. All identifiers follow the project's `snake_case` convention (`code-standards.md`); ports do not carry an `I*` or `_interface` suffix (`tpm-write-code` "Naming"). For component namespaces, document the unprefixed type inside the namespace, such as `tpmkit::pcr::provider`, not a root-level `pcr_provider`.

### Public headers

```cpp
/**
 * @class tpm_esys_context
 * @brief C++ wrapper for ESAPI context management with dependency injection.
 *
 * Provides a safe interface for managing an ESYS context and performing TPM
 * operations through the Enhanced System API. Handles initialization, resource
 * management, and cleanup of both ESYS and TCTI contexts. Supports dependency
 * injection of the API and error decoder via tpm_esys_context_options.
 *
 * Key features:
 * - Automatic ESYS and TCTI context lifetime management.
 * - Dependency injection via tpm_esys_context_options for the API and error decoder.
 * - Factory methods for flexible construction (by TCTI name or existing TCTI context).
 * - Construction failures throw; runtime calls return outcome<T, error>.
 * - Thread-compatible: independent instances are safe; a single instance must not be shared.
 *
 * Typical usage:
 * @code
 * // Default options (tabrmd TCTI, clear startup).
 * auto ctx = tpm_esys_context::create();
 *
 * // Explicit TCTI and startup mode.
 * auto ctx2 = tpm_esys_context::create("device", tpm_startup_mode::state);
 *
 * // Inject custom API and error decoder for tests.
 * tpm_esys_context_options opts;
 * opts.api = std::make_shared<mock_esys_api>();
 * opts.error_decoder = std::make_shared<mock_error_decoder>();
 * auto ctx3 = tpm_esys_context::create("tabrmd", tpm_startup_mode::clear, opts);
 *
 * auto rnd = ctx.get_random(32);
 * @endcode
 *
 * @note The first instance performs TPM startup; subsequent instances reuse the
 *       initialized TPM. The startup-mode argument therefore only affects the
 *       first context created.
 *
 * @see tpm_esys_context_options for dependency-injection configuration.
 * @see tpm_tcti_context for TCTI configuration and management.
 * @see esys_api for the abstract API port.
 */
class tpm_esys_context final {
    // ...
};
```

What's load-bearing in the public template:

- `@brief` is one sentence, stands alone in a generated index.
- The description paragraph names *what the type does* and the major collaborators.
- "Key features" enumerates contracts a caller needs to know before reading the methods — error model (exceptions vs. `outcome`), thread-safety, lifetime, injection seams.
- "Typical usage" shows the happy path *and* the test-injection seam, because both are part of the contract.
- `@note` calls out non-obvious behavior — startup ordering, persistent-handle ownership, anything a careful reader would want to know before depending on it.
- `@see` links to the closest collaborators and the abstract port the type implements or consumes. Don't enumerate every concrete adapter; one port reference is enough.

### Internal headers

Internal-header types use a slimmer form — drop "Key features", "Typical usage", and `@see` chains; keep `@brief`, the description, and `@note`s for non-obvious behavior. Mark them as adapter-internal so a reader knows they're not part of the public contract.

```cpp
/**
 * @brief Translates TSS2_RC values into the domain error category.
 *
 * Owns the mapping table from TSS2_RC to error::category. The translation is
 * deterministic — the same TSS2_RC always maps to the same category. Codes that
 * have no explicit mapping fall through to category::backend_error.
 *
 * @note Adapter-internal. Not part of the public API.
 */
class tss2_error_translator {
    // ...
};
```

## Internal documentation

- Adapters document the third-party library version they target and the translation rules they apply (which `TSS2_RC` maps to which domain error category).
- Non-obvious invariants and lifetime contracts are documented at the declaration, not at the implementation. Implementation comments are reserved for "why," never "what."
- Avoid restating what the code already says (cross-reference: `.claude/rules/code-standards.md` Comments).

## Project-level documentation

- **README.md** — what the library does, supported platforms, minimum compiler and dependency versions, build-time options such as `TPMKIT_ENABLE_LEGACY_SHA1_PCR`, quick-start, links to examples and full docs.
- **CHANGELOG.md** — Keep a Changelog format. Every release entry lists Added / Changed / Deprecated / Removed / Fixed / Security. Security fixes are called out explicitly with the CVE if any.
- **examples/** — runnable code that exercises common use cases. Each example builds in CI; broken examples are a release blocker.
- **docs/adr/** — Architecture Decision Records for non-obvious choices (hexagonal layout, FAPI vs. ESYS adapter strategy, choice of error type). Format: title, context, decision, consequences. One file per decision; never edited after acceptance — supersede with a new ADR instead.

## Style

- Documentation is in English (cross-reference: `.claude/rules/code-standards.md`).
- Code samples in documentation must compile. CI builds extracted samples from the README and Doxygen comments.
- Prefer showing intent through small examples over prose explanations.

## Error Handling

* **Doxygen-coverage CI fails on a public symbol.** Add the missing `@brief` and full block per the "Doxygen templates" public-header form. Do not annotate the symbol as `@internal` to silence the check — `@internal` is for genuinely internal types in public headers, not a coverage workaround.
* **Extracted code sample fails to build.** Fix the sample (real, up-to-date types) or move it out of `@code`/`@endcode` and into prose. A broken sample is worse than a missing one; it teaches the wrong API.
* **Overloads have drifted to near-duplicate Doxygen blocks.** Promote the most general overload to the canonical block and reduce the others to `@copydoc <full_signature>` plus only the delta. Drift is a maintenance bug — one canonical block beats two near-duplicates.
* **Public enum is documented at the type level but enumerators are bare.** Add a one-line comment to every enumerator. The values *are* the contract; consumers cannot infer them from the enum's name alone.
* **Deprecation lacks a removal version.** Pair `[[deprecated("use X instead")]]` with `@deprecated Since vX.Y; use X instead. Removed in vZ.0.` per `library-api-design.md` Versioning and deprecation. A deprecation without a removal target rots into permanent surface.
* **ADR appears to need editing after acceptance.** Do not edit. Supersede with a new ADR that references the old one. ADRs are an append-only decision log; editing destroys the audit trail.
* **README compatibility table conflicts with `tpm-build-config` Supported toolchains.** `tpm-build-config` wins. Update the README in the same PR that changes the supported set.
* **CHANGELOG entry uses a category not in Keep a Changelog.** Restrict to Added / Changed / Deprecated / Removed / Fixed / Security. Cross-reference `tpm-release` for the release-time promotion procedure.
