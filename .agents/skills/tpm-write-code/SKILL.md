---
name: tpm-write-code
description: Implementation guidance for the tpmkit C++17 library — applying SOLID and hexagonal architecture in practice, modeling the domain with value objects, entities, and domain services from DDD, and a curated catalog of design patterns (Strategy, Factory, RAII wrapper, Pimpl, Adapter, Visitor, Null Object, Template Method) tied to the codebase. Use when writing or refactoring production source code under `src/` or `include/<library>/`, deciding where new code belongs (domain vs. adapter vs. composition root), introducing a new port, modeling a new domain concept, picking a design pattern, or tackling a refactor. Do not use for writing tests, build/CMake/CI configuration, documentation, or commits/PRs.
---

# Writing Code — tpmkit

This skill guides production code under `src/` and `include/<library>/`. The always-on rules — `.claude/rules/code-standards.md`, `architecture.md`, `library-api-design.md`, `security.md`, `error-handling.md` — are authoritative on baseline rules. This skill focuses on the *judgment calls* those rules don't make for you: where new code goes, how to model it, and which pattern fits.

## Where does new code belong?

Decide before you start typing. Ask in order:

1. **Does the code depend on a third-party library** (OpenSSL, TPM2 TSS, OS calls)? → adapter, under `src/adapters/<name>/`. When adding a backend for an existing adapter family, use `src/adapters/<family>/<backend>/` instead; logging backends live under `src/adapters/logging/<backend>/`.
2. **Is it a concept or rule that exists independently of any backend** (a digest, a signature, a policy decision)? → domain, under `src/domain/` or `include/<library>/`.
3. **Does it choose between adapters and wire them together?** → composition root, under `src/composition/`.

A function that opens a TPM session belongs in an adapter. A function that decides whether a signature is well-formed before sending it to verify belongs in the domain. A function that picks FAPI vs. ESYS at startup belongs in composition.

If you find yourself wanting to include an `openssl/` or `tss2/` header from a domain file, stop and extract a port instead. The dependency must point inward (`architecture.md`).

When wiring a new backend, look up the closest precedent before inventing one: `references/worked-examples.md` walks through how OpenSSL (build-time selection) and TPM2 TSS (runtime adapter selection) are laid out — file paths, CMake wiring, and the FAPI/ESYS-as-internal-detail rule.

## Naming

The codebase uses settled patterns. Match them when adding new types so existing readers don't have to context-switch.

- **Ports** — noun describing the capability the domain needs: `key_provider`, `crypto_primitives`, `logger`. Avoid verb-based names (`signs_keys`) and suffixes like `_port` or `_interface` — the abstract base class *is* the port.
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

Distinguish from ports: a *port* expresses what the domain needs from the outside world (`key_provider`, `crypto_primitives`). A *domain service* is logic that lives entirely inside the domain, possibly using ports to do its work.

## SOLID, applied

The code-standards rules give you function- and class-level basics. SOLID is how those scale to a module:

- **S**ingle responsibility — a class has one reason to change. If you describe a class with "and," split it. A class that "validates the policy *and* serializes the policy bytes" is two classes.
- **O**pen/closed — extend behavior without modifying existing code. New algorithm support arrives as a new adapter or a new entry in a Strategy registry, not by editing a `switch` in the domain.
- **L**iskov substitution — every adapter behind a port satisfies the same observable contract. The contract test suite (`tests/contract/`) enforces this; if a real adapter throws where the mock returns an outcome, the mock is wrong or the contract is unspecified — fix one of them.
- **I**nterface segregation — ports are narrow. A `key_provider` does not also expose logging hooks or metrics. If two callers want disjoint methods on the same port, split into two ports.
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

When the domain needs one of several backends and the choice is a runtime decision (hardware-present vs. software fallback, FAPI vs. ESYS), declare a port and write each backend as an adapter. This is the shape `key_provider` already takes. Use it when a mock is needed for tests, when runtime fallback is a real requirement, or when integration tests must substitute the implementation.

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

The mock `key_provider` and the no-op `logger` adapter are Null Objects: they satisfy the port without doing the underlying work. They simplify call sites — there are no `if (logger)` checks scattered around — and let the same domain code run with logging disabled, without a TPM, or under tests. Use the pattern whenever the alternative would be `nullptr` checks at every call site.

### Template Method — fixed skeleton, varying steps

Use sparingly. When several adapters share the same outline (open session, do work, close session) but differ in the work step, a small base class can capture the skeleton with a virtual hook for the variant. Prefer composition over inheritance by default; only reach for Template Method when the skeleton genuinely *is* the abstraction.

## Common workflows

Recipes for the most frequent kinds of change. Each is sequenced so the contract test suite catches mismatches *before* they reach production code.

### Adding a method to an existing port

1. Add the abstract method to the port in `src/domain/<port>.h`.
2. Add the matching method to the **mock adapter** (`src/adapters/mock/`). The mock is the cheapest place to think through the contract.
3. Extend the **contract test suite** with a test for the new method. It should pass against the mock first.
4. Implement the method in every **real adapter**. Run the contract suite against each.
5. Update callers in the domain.
6. If the method takes a new domain type, follow "Promoting a primitive to a value object" first.

If the method is meaningful for some adapters but not others, that is a smell — split the port (interface segregation) rather than adding a "throw not_supported" stub.

### Introducing a new port

1. Sketch the port's methods in `src/domain/<port>.h` — domain types only, no third-party headers.
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

### Adding a new backend adapter for an existing port

1. Create `src/adapters/<backend>/`. Match the naming pattern from "Naming" above.
2. Subclass the port, implement every method, translate third-party errors at the boundary.
3. Run the **contract suite** against the new adapter *before* wiring it into the composition root. Adapters that pass the contract suite are interchangeable; adapters that don't are bugs waiting to happen.
4. Wire into the composition root with an explicit selection rule (configuration flag, hardware-presence check). The selection logic is the only place that names the concrete type.

## Refactoring triggers

These signals usually mean a design change, not a tweak to a function:

- A class file crosses 300 lines with no obvious split — start by extracting a value object or a helper service.
- A `switch` or `if` ladder over a type tag — replace with polymorphism (port + adapters) or `std::visit`.
- A function's parameter list grows to four or more — group related parameters into a value object first; if that doesn't help, the function is doing too much.
- A primitive (`uint32_t`, `std::vector<uint8_t>`, `std::string`) recurs everywhere with the same validation — promote to a value object once, validate once.
- A second adapter is being copy-pasted from the first — the duplication is your port; extract it.

## Anti-patterns to flag

These recur in C++ crypto and TPM code:

- Catching `std::exception` only to log and rethrow. Either let it propagate or translate it; logging-as-passthrough adds noise without adding value.
- Returning `bool` for "operation succeeded" when there are several distinct failure modes — use `outcome<T, error>` (`error-handling.md`).
- Two-phase construction (`make()` + `init()`). Either the constructor establishes invariants or the type isn't usable yet.
- A constructor that internally `new`s an adapter (or grabs a global) instead of taking a port as a parameter. Welds the class to one backend, kills testability, and breaks dependency inversion. Inject the port; let the composition root choose the concrete type.
- A domain header transitively pulling in `openssl/` or `tss2/`. Break the chain with a forward declaration or by moving the include into the `.cpp`.
- "Util" or "helper" classes that accumulate unrelated free functions. Find their real homes — usually as value-object methods or domain services.
- Re-validating internal-call inputs. Trust internal callers (`code-standards.md`); validate only at the public API boundary.

## Error Handling

* **A domain file needs to include `openssl/` or `tss2/`.** Stop. The dependency must point inward (`architecture.md`). Either extract a port the domain depends on and put the include in the appropriate adapter folder (`src/adapters/<name>/`, or a grouped family such as `src/adapters/logging/<backend>/`), or replace the third-party type in the signature with a domain type. Do not relax the grep gate documented in `tpm-build-config` Invariant checks.
* **A class crosses 300 lines without an obvious split.** Refactoring trigger. Look for a value object hiding inside the data, a domain service hiding inside the methods, or a port hiding inside a `switch` over a type tag. Adding "helper" private methods to absorb the growth makes the file longer without addressing the single-responsibility violation.
* **A new method fits some adapters but not others.** Interface segregation problem — split the port. Do not add a "throw not_supported" stub; routing it through `tpm-add-port-or-adapter` Workflow A for the split is the right path.
* **Contract suite passes against the mock but fails against a real adapter.** The mock under-specifies the contract. Tighten the mock so the contract matches observed real-adapter behavior (error categories, lifecycle constraints, threading), then re-run against every adapter, including the mock.
* **A `secret_buffer` was copied accidentally** (compile error: deleted copy constructor). That is the design working as intended. Move instead, or extract a non-secret view if the consumer only needs a span over the bytes for the duration of the call.
* **A `switch` over an `enum class` grows a new case and no compiler warning fires.** Add a `default: static_assert(false, ...)` or switch to `std::visit` over a `std::variant`. Cross-reference *Visitor* in the pattern catalog — the whole point is that adding a new case fails to compile until every site handles it.
* **A new value object breaks public-API compatibility.** That is an ABI break. Mark the commit with `!` (per `tpm-commit-pr` Commits), update the CHANGELOG with the migration path, and trigger the SemVer major bump per `library-api-design.md` Versioning.
* **Tests pass but the refactor "feels wrong".** Trust the unease, but verify mechanically: run the contract suite against every adapter, run the sanitizer matrix, and check that the public API surface is unchanged via the symbol-export diff (`tpm-build-config` Invariant checks). If all three pass, the refactor is sound.
