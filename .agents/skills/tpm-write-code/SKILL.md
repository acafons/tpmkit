---
name: tpm-write-code
description: Implementation guidance for the tpmkit C++17 library — applying SOLID and hexagonal architecture, modeling the domain with value objects/entities/services, choosing local vs. public API placement, and using established design patterns. Use when writing/refactoring production source under `src/` or `include/<library>/`, deciding where code belongs, evaluating whether helpers/classes from examples/tests/adapters should become public API or stay local, introducing a port, modeling a domain concept, picking a pattern, or tackling a refactor. Do not use for writing tests, build/CMake/CI configuration, documentation, or commits/PRs.
---

# Writing Code — tpmkit

This skill guides production code under `src/` and `include/<library>/`, and the public-API candidacy check for reusable helpers discovered in examples, tests, adapters, or domain internals. The always-on rules — `.claude/rules/code-standards.md`, `architecture.md`, `library-api-design.md`, `security.md`, `error-handling.md` — are authoritative on baseline rules. This skill focuses on the *judgment calls* those rules don't make for you: where new code goes, how to model it, when it deserves public surface, and which pattern fits.

## Where does new code belong?

Decide before you start typing. Ask in order:

1. **Does the code depend on a third-party library** (OpenSSL, TPM2 TSS, OS calls)? → adapter, under `src/adapters/<name>/`. When adding a backend for an existing adapter family, use `src/adapters/<family>/<backend>/` instead; logging backends live under `src/adapters/logging/<backend>/`.
2. **Is it a concept or rule that exists independently of any backend** (a digest, a signature, a policy decision)? → domain, under `src/domain/` or `include/<library>/`.
3. **Does it choose between adapters and wire them together?** → composition root, under `src/composition/`.

A function that opens a TPM session belongs in an adapter. A function that decides whether a signature is well-formed before sending it to verify belongs in the domain. A function that picks FAPI vs. ESYS at startup belongs in composition.

When a domain area is expected to grow, give it a real component namespace and
matching folders. Public headers go under `include/tpmkit/<area>/`,
implementation files go under `src/domain/<area>/`, and public names live under
`tpmkit::<area>`. PCR is the reference shape:
`include/tpmkit/pcr/provider.h`, `src/domain/pcr/selection.cpp`, and
`tpmkit::pcr::provider`. Future NV and key APIs should follow the same pattern,
for example `tpmkit::nv::index` or `tpmkit::key::provider`.

When an adapter implements a growing component port, keep the concrete provider
file as an orchestration layer. It should validate, marshal, call the backend,
translate errors, log, and notify observers by composing focused helper modules;
it should not define all conversion, validation, logging, authorization, and
resource helper logic inline. For TPM2 ESYS, follow the PCR shape:
`src/adapters/tpm2_esys/<area>/<area>_marshalling.{h,cpp}`,
`<area>_validation.{h,cpp}`, and `<area>_logging.{h,cpp}` as needed, with
shared context lifecycle under `context/` and shared TSS support under
`support/`. Start this split before the provider crosses roughly 300 lines or
mixes three responsibilities beyond command orchestration.

If you find yourself wanting to include an `openssl/` or `tss2/` header from a domain file, stop and extract a port instead. The dependency must point inward (`architecture.md`).

When wiring a new backend, look up the closest precedent before inventing one: `references/worked-examples.md` walks through how OpenSSL (build-time selection) and TPM2 TSS (runtime adapter selection) are laid out — file paths, CMake wiring, and the FAPI/ESYS-as-internal-detail rule.

## Public API candidate check

Run this check before adding or keeping a helper/class in `examples/`, `tests/`,
adapter-local code, an anonymous namespace, or `src/domain/` when the symbol has
domain meaning. Also run it during refactors that discover duplicated helper
logic. Public API is a compatibility promise, so the default is analysis first,
not automatic promotion.

Classify the symbol as one of four outcomes:

- **Public API** — stable domain vocabulary or caller workflow support that a
  consumer would reasonably need to use the library correctly, safely, or
  without reimplementing library-owned rules.
- **Domain-internal** — reusable inside the library, backend-neutral, and owned
  by the domain, but not stable enough or useful enough to install as API.
- **Adapter-internal** — translation, ABI glue, lifecycle handling, or formatting
  that belongs to one backend or third-party library boundary.
- **Example/test-local** — demo input preparation, CLI scaffolding, fixture
  setup, or assertions that should not become a consumer contract.

Ask these questions before choosing:

- Is equivalent logic duplicated in examples, tests, adapters, or
  downstream-like smoke code?
- Would a consumer otherwise need to copy the logic to call the public API
  correctly or interpret its results?
- Does the symbol name a stable domain concept, representation, or workflow,
  rather than demo/test glue?
- Can it live under a backend-neutral header and namespace that match
  `library-api-design.md`?
- Can the contract be documented precisely: inputs, ownership, validation,
  returned error categories, exceptions, thread safety, lifetime, and secret
  handling?
- Is the behavior stable enough for SemVer compatibility, ABI/export rules, and
  future deprecation discipline?
- Does promotion remove meaningful reimplementation without bloating the public
  surface?

Promote only when the answers are coherent. Keep the symbol local when the value
is convenience inside one example, one test fixture, or one backend. Examples:
`hash_algorithm_name`, `error_category_name`, `encoding::encode_hex`,
`encoding::decode_hex`, `pcr::make_index_range`, and the string TCTI
`tpm_context::create` overload are public-API candidates because they represent
stable caller-facing vocabulary or workflows. `examples::make_event_bytes` stays
example-local because it is demo input construction, not library behavior.

When the decision is not obvious, leave a short rationale in the implementation
notes, commit message, or PR: "promoted because consumers would otherwise
reimplement X" or "kept example-local because it only prepares demo input."

## Naming

The codebase uses settled patterns. Match them when adding new types so existing readers don't have to context-switch.

- **Ports** — noun describing the capability the domain needs: `provider`
  inside a component namespace, `crypto_primitives`, `logger`. Avoid
  verb-based names (`signs_keys`) and suffixes like `_port` or `_interface` —
  the abstract base class *is* the port.
- **Ports inside component namespaces** — use the unprefixed role name inside
  the area namespace: `tpmkit::pcr::provider`, `tpmkit::nv::provider`,
  `tpmkit::key::provider`. Do not use root-level prefixed types such as
  `pcr_provider`, and do not double the prefix as `pcr::pcr_provider`.
- **Adapters** — `<backend>_<port>` shape: `tpm2_fapi_adapter`, `mock_key_provider`, `software_key_provider`. Use the `_adapter` suffix when the backend name doesn't already disambiguate.
- **Value objects** — name the concept, not the byte layout: `digest_sha256`, `signature`, `algorithm_id`. The size lives in the type, not the name.
- **Entities** — name the domain concept (`tpm_session`, `loaded_key`), not the storage (`session_record`).
- **RAII wrappers for C resources** — name after the resource: `tss2_context`, `evp_pkey`. Not `tss2_context_wrapper` — the type *is* the wrapper.
- **Domain services** — verb function or `_service` class: free `sign(...)` / `verify(...)`, or `policy_evaluator` when state is involved.

## Modeling the domain

The domain is built from three kinds of building block. Tell them apart by asking what defines equality. For concrete skeletons of each shape, see `references/examples.md` (sections 1–3).

### Value objects — equality by content

Use for any domain concept that is fully described by its data: digests, signatures, algorithm identifiers, key-handle indices, byte counts with semantics, public key bytes. Two equal byte sequences mean the same value.

Properties:
- Immutable after construction. No setters.
- Constructor validates invariants and throws on violation (e.g., `digest_sha256` is exactly 32 bytes; `algorithm_id` is one of an allowed set).
- Equality and hashing by content.
- No identity, no lifecycle.

Reach for one whenever you would otherwise pass a `std::vector<uint8_t>`, `uint32_t`, or `std::string` whose meaning is domain-specific. The named type carries the invariant in the type system; the primitive does not.

### Entities — equality by identity

Use for concepts that have a lifecycle and identity independent of their state: a TPM key (identified by its persistent or transient handle), an open session, a loaded policy. Two keys with identical bytes but different handles are different entities.

Properties:
- Carry an explicit identity field.
- May change state over their lifetime (a session's nonce updates; a key's loaded/unloaded status changes).
- Equality compares identity, never state.

If you can't articulate what identity *is* for a candidate entity, it is probably a value object.

### Domain services — operations that don't fit on one entity

Some operations span entities or take only value objects: `sign(key, message)`, `verify(public_key, signature, message)`, `derive_key(parent, label)`. These belong in a stateless function or a thin service class, not bolted onto an entity that "happens to be involved."

Distinguish from ports: a *port* expresses what the domain needs from the outside world (`pcr::provider`, `crypto_primitives`). A *domain service* is logic that lives entirely inside the domain, possibly using ports to do its work.

## SOLID, applied

The code-standards rules give you function- and class-level basics. SOLID is how those scale to a module:

- **S**ingle responsibility — a class has one reason to change. If you describe a class with "and," split it. A class that "validates the policy *and* serializes the policy bytes" is two classes.
- **O**pen/closed — extend behavior without modifying existing code. New algorithm support arrives as a new adapter or a new entry in a Strategy registry, not by editing a `switch` in the domain.
- **L**iskov substitution — every adapter behind a port satisfies the same observable contract. The contract test suite (`tests/contract/`) enforces this; if a real adapter throws where the mock returns an outcome, the mock is wrong or the contract is unspecified — fix one of them.
- **I**nterface segregation — ports are narrow. A `key::provider` does not also expose logging hooks or metrics. If two callers want disjoint methods on the same port, split into two ports.
- **D**ependency inversion — the domain declares ports; adapters depend on them. Never the reverse.

## One thing per function

A function does one thing if you can describe what it does in a sentence without using "and." When you can't:

- Extract the side-effect from the calculation. A function that computes *and* logs is two functions.
- Extract the validation from the work. A function that validates input *and* signs the message is two functions.
- Extract the loop body. If the body is non-trivial, name it.

Smell signals: more than 20 lines, more than 3 parameters, more than two levels of nesting, a boolean parameter that switches behavior, or comments that read like section headers (`// now do the second part`). Each usually means a missing function.

## Design pattern catalog

Patterns here are the ones that recur in tpmkit. Reach for them when the situation matches; do not retrofit them onto code that is already clear. For code skeletons of the shape-driven patterns (RAII wrapper, Port + Adapter, Visitor, Null Object), see `references/examples.md` (sections 4–7); for Pimpl mechanics see `references/pimpl-mechanics.md`.

### Strategy — runtime choice between interchangeable algorithms

When the domain needs one of several backends and the choice is a runtime decision (hardware-present vs. software fallback, FAPI vs. ESYS), declare a port and write each backend as an adapter. This is the shape `pcr::provider` takes. Use it when a mock is needed for tests, when runtime fallback is a real requirement, or when integration tests must substitute the implementation.

### Factory — building configured objects

The composition root is a Factory. It assembles an adapter with its dependencies, configuration, and connection setup, and hands it back as the port type. Keep factories close to the entry point — every binary, test, or example has its own composition root. The domain never instantiates an adapter directly.

When a factory creates a component from an existing owner/context, it is not a new composition root. Prefer a backend-neutral member factory on the owner (`ctx.create_pcr_provider()`), return the domain port type, and keep backend names such as ESYS/FAPI out of the public API. Reuse the owner's effective logger instead of adding another logger parameter; see `tpm-write-logging` "Owning the logger" for the context-derived component rule. Do not add public raw-handle accessors or friend/free-function seams solely to wire an adapter.

### RAII wrapper — owning a C-library resource

Every TSS2 context, OpenSSL `EVP_*` handle, file descriptor, or mutex lock has a destructor that releases it. For one-off resources, `std::unique_ptr<T, deleter>` is enough. For resources used in many places, write a dedicated class with deleted copy and defined move; the wrapper has one job: ensure release on scope exit. Manual `_free` calls at use sites are an anti-pattern.

### Pimpl — insulating ABI from implementation changes

Public classes with non-trivial internals use the Pimpl idiom (`library-api-design.md`). The header forward-declares an `impl` struct and holds it via `std::unique_ptr`; the `.cpp` defines `impl`. This keeps adapter headers out of the public surface and keeps consumer rebuilds cheap when internals change. For the mechanics — destructor placement, move/copy semantics, migration steps — see `references/pimpl-mechanics.md`.

### Adapter — translating between two interfaces

Every concrete `*_adapter` class is the Adapter pattern: it implements a domain port in terms of a third-party API. Translation is the adapter's job and only the adapter's job — error codes, types, and lifetimes get translated at this boundary so the domain sees only domain types and domain errors.

### Visitor — operating over a closed type set

Use Visitor (or `std::visit` over a `std::variant`) when the domain has a fixed set of cases (algorithm identifier, policy node type) and you want exhaustive handling that fails to compile when a new case is added. Prefer this over `dynamic_cast` chains or untyped `if`/`else if` on a type tag.

### Null Object — uniform handling of "absent"

The mock `pcr::provider` and the no-op `logger` adapter are Null Objects: they satisfy the port without doing the underlying work. They simplify call sites — there are no `if (logger)` checks scattered around — and let the same domain code run with logging disabled, without a TPM, or under tests. Use the pattern whenever the alternative would be `nullptr` checks at every call site.

### Template Method — fixed skeleton, varying steps

Use sparingly. When several adapters share the same outline (open session, do work, close session) but differ in the work step, a small base class can capture the skeleton with a virtual hook for the variant. Prefer composition over inheritance by default; only reach for Template Method when the skeleton genuinely *is* the abstraction.

## Common workflows

Recipes for the most frequent kinds of change. Each is sequenced so the contract test suite catches mismatches *before* they reach production code.

### Adding a method to an existing port

1. Add the abstract method to the port in its component header, such as
   `include/tpmkit/pcr/provider.h`.
2. Add the matching method to the **mock adapter** (`src/adapters/mock/`). The mock is the cheapest place to think through the contract.
3. Extend the **contract test suite** with a test for the new method. It should pass against the mock first.
4. Implement the method in every **real adapter**. Run the contract suite against each.
   If a real provider file now contains backend/domain marshalling, validation,
   logging, and command dispatch in the same translation unit, split those
   helpers before adding more methods.
5. Update callers in the domain.
6. If the method takes a new domain type, follow "Promoting a primitive to a value object" first.

If the method is meaningful for some adapters but not others, that is a smell — split the port (interface segregation) rather than adding a "throw not_supported" stub.

### Introducing a new port

1. Choose the component area first. If the capability belongs to a growing area,
   put the public port under `include/tpmkit/<area>/provider.h` in
   `namespace tpmkit::<area>`; keep implementation helpers under
   `src/domain/<area>/`. Use domain types only, no third-party headers.
2. Implement the **mock adapter first**. If the mock is awkward to write, the port shape is wrong; iterate before going further.
3. Write the **contract test suite** against the mock. Cover the happy path and each error category from `error-handling.md`.
4. Implement the **real adapter(s)**, translating third-party errors at the boundary.
5. Run the contract suite against every real adapter. They must pass identically — that's what makes them interchangeable.
6. Wire into the composition root.

### Promoting a primitive to a value object

When a primitive (`uint32_t`, `std::vector<uint8_t>`, `std::string`) recurs everywhere with the same validation:

1. Define the value object — invariants in the constructor, equality by content (`references/examples.md` section 1).
2. Replace the primitive at one call site at a time, working from the public API inward so the new type's reach grows from the boundary.
3. Delete the redundant validation that used to live at each call site — the type now carries the invariant.
4. Tests should be unchanged in spirit but tighter in spelling.

### Promoting a helper or class to public API

Use this workflow only after the public API candidate check says promotion is
right. If the check points to domain-internal, adapter-internal, or
example/test-local, keep the symbol there and record the rationale if future
readers are likely to ask why.

1. Choose the public home first: root `include/tpmkit/` for shared foundational
   vocabulary, or `include/tpmkit/<area>/` and `tpmkit::<area>` for a cohesive
   component family. Do not expose backend names, example names, test names, or
   generic `util`/`helper` buckets.
2. Design the contract before moving code: name, parameter ownership, valid
   ranges, error categories or exception types, exception-safety guarantee,
   thread-safety contract, lifetime rules, and any secret-handling restrictions.
3. Put non-trivial implementation in `src/domain/` or `src/domain/<area>/`.
   Keep public headers free of third-party includes and avoid inline bodies
   unless the function is genuinely trivial and ABI-safe.
4. Add the matching public API tests and header smoke coverage per
   `tpm-write-tests`. For parsers, encoders, and boundary-sensitive helpers,
   include malformed input and size/overflow cases.
5. Add Doxygen on the public declaration per `tpm-write-docs`: parameters,
   return value, errors, exceptions, thread safety, and `@since`.
6. Replace duplicate example/test/adapter implementations with the public API
   and delete the old helper. The examples should teach the library API, not
   carry a second private version of it.

### Adding a new backend adapter for an existing port

1. Create `src/adapters/<backend>/`. Match the naming pattern from "Naming" above. If the adapter has multiple port methods or is likely to grow, create responsibility subfolders and helper files up front instead of starting with one large provider translation unit.
2. Subclass the port, implement every method, translate third-party errors at the boundary. Keep the concrete provider focused on command sequencing; put marshalling, validation, logging, session setup, and resource helpers in adapter-internal modules.
3. Run the **contract suite** against the new adapter *before* wiring it into the composition root. Adapters that pass the contract suite are interchangeable; adapters that don't are bugs waiting to happen.
4. Wire into the composition root with an explicit selection rule (configuration flag, hardware-presence check). The selection logic is the only place that names the concrete type.

## Refactoring triggers

These signals usually mean a design change, not a tweak to a function:

- A class file crosses 300 lines with no obvious split — start by extracting a value object or a helper service.
- A concrete provider file is becoming a catalog of all port methods plus all
  marshalling, validation, and logging helpers — keep provider methods as
  command orchestration and extract helper modules by responsibility.
- A `switch` or `if` ladder over a type tag — replace with polymorphism (port + adapters) or `std::visit`.
- A function's parameter list grows to four or more — group related parameters into a value object first; if that doesn't help, the function is doing too much.
- A primitive (`uint32_t`, `std::vector<uint8_t>`, `std::string`) recurs everywhere with the same validation — promote to a value object once, validate once.
- An example/test/helper implementation is copied into a second place, or an
  adapter duplicates a domain-facing mapping already visible to callers — run
  the public API candidate check before adding another copy.
- A second adapter is being copy-pasted from the first — the duplication is your port; extract it.

## Anti-patterns to flag

These recur in C++ crypto and TPM code:

- Catching `std::exception` only to log and rethrow. Either let it propagate or translate it; logging-as-passthrough adds noise without adding value.
- Returning `bool` for "operation succeeded" when there are several distinct failure modes — use `outcome<T, error>` (`error-handling.md`).
- Two-phase construction (`make()` + `init()`). Either the constructor establishes invariants or the type isn't usable yet.
- A constructor that internally `new`s an adapter (or grabs a global) instead of taking a port as a parameter. Welds the class to one backend, kills testability, and breaks dependency inversion. Inject the port; let the composition root choose the concrete type.
- A domain header transitively pulling in `openssl/` or `tss2/`. Break the chain with a forward declaration or by moving the include into the `.cpp`.
- "Util" or "helper" classes that accumulate unrelated free functions. Find their real homes — usually as value-object methods or domain services.
- Leaving a domain-shaped helper in `tpmkit::examples` after examples, tests,
  or adapter code start depending on the same behavior. Either promote it with a
  public contract or keep each local copy deliberately scoped to demo/test glue.
- Promoting demo scaffolding or test fixture setup just to remove a small amount
  of duplication. Public API carries SemVer, ABI, docs, tests, and security
  commitments; convenience alone is not enough.
- Re-validating internal-call inputs. Trust internal callers (`code-standards.md`); validate only at the public API boundary.

## Error Handling

* **A domain file needs to include `openssl/` or `tss2/`.** Stop. The dependency must point inward (`architecture.md`). Either extract a port the domain depends on and put the include in the appropriate adapter folder (`src/adapters/<name>/`, or a grouped family such as `src/adapters/logging/<backend>/`), or replace the third-party type in the signature with a domain type. Do not relax the grep gate documented in `tpm-build-config` Invariant checks.
* **A class crosses 300 lines without an obvious split.** Refactoring trigger. Look for a value object hiding inside the data, a domain service hiding inside the methods, or a port hiding inside a `switch` over a type tag. Adding "helper" private methods to absorb the growth makes the file longer without addressing the single-responsibility violation.
* **A provider or adapter translation unit crosses 300 lines or mixes command dispatch with conversion tables, validation, logging, and resource helpers.** Split it before adding the next method. For TPM2 ESYS component adapters, use the PCR precedent: `<area>_marshalling`, `<area>_validation`, and `<area>_logging` in the component folder, with cross-component TSS support in `support/`.
* **A helper looks reusable but the public contract is not stable.** Do not promote it yet. Keep it domain-internal, adapter-internal, or example/test-local according to the candidate check, and record the reason if duplication is likely to tempt a future promotion.
* **An example-local helper is about to be copied into an adapter, test, or second example.** Stop and run the public API candidate check. Promote it when it is stable caller-facing behavior; otherwise keep the duplicate intentionally local and narrow.
* **A new method fits some adapters but not others.** Interface segregation problem — split the port. Do not add a "throw not_supported" stub; routing it through `tpm-add-port-or-adapter` Workflow A for the split is the right path.
* **Contract suite passes against the mock but fails against a real adapter.** The mock under-specifies the contract. Tighten the mock so the contract matches observed real-adapter behavior (error categories, lifecycle constraints, threading), then re-run against every adapter, including the mock.
* **A `secret_buffer` was copied accidentally** (compile error: deleted copy constructor). That is the design working as intended. Move instead, or extract a non-secret view if the consumer only needs a span over the bytes for the duration of the call.
* **A `switch` over an `enum class` grows a new case and no compiler warning fires.** Add a `default: static_assert(false, ...)` or switch to `std::visit` over a `std::variant`. Cross-reference *Visitor* in the pattern catalog — the whole point is that adding a new case fails to compile until every site handles it.
* **A new value object breaks public-API compatibility.** That is an ABI break. Mark the commit with `!` (per `tpm-commit-pr` Commits), update the CHANGELOG with the migration path, and trigger the SemVer major bump per `library-api-design.md` Versioning.
* **Tests pass but the refactor "feels wrong".** Trust the unease, but verify mechanically: run the contract suite against every adapter, run the sanitizer matrix, and check that the public API surface is unchanged via the symbol-export diff (`tpm-build-config` Invariant checks). If all three pass, the refactor is sound.
