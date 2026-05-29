---
name: tpm-add-port-or-adapter
description: Mechanics for introducing a new port or adapter in the tpmkit C++17 hexagonal architecture — file layout, polymorphism choice (runtime/compile-time/build-time), CMake wiring, mock-first contract test workflow, and composition-root integration. Use when adding a brand-new port to the domain, adding a new backend adapter for an existing port, or migrating an existing collaborator behind a port. Do not use for changing logic inside an existing adapter, refactoring within the domain core, or test-only mock additions that do not change the port surface.
---

# Adding a Port or Adapter — tpmkit

This skill covers the *mechanical* steps for introducing a new port (a new abstraction the domain depends on) or a new adapter (a new backend behind an existing port). The judgment calls — when a port is the right shape, what concept it should model — live in `tpm-write-code` and `architecture.md`. This skill assumes that decision is made.

The skill is organised around two workflows. Pick the one that matches the change in front of you.

## Workflow A — Adding a brand-new port

Use this workflow when the domain needs a new capability from the outside world (a new external system, a new kind of provider, a new policy source).

### A.1 Choose the polymorphism mechanism

Decide before you create files. Three options exist (see `architecture.md`); the choice constrains every later step.

| Need | Option | Cost |
|---|---|---|
| Mock for tests, runtime fallback, hardware-presence detection | **1. Runtime polymorphism** (abstract base + virtual methods) | One virtual dispatch per call. |
| Hot path where virtual dispatch is measurable, no test-time substitution | **2. Compile-time polymorphism** (template + concept-like SFINAE) | Adapter headers leak into domain headers. Cannot use GMock. |
| One backend per build, no test-time substitution | **3. Build-time selection** (one header, one `.cpp` per adapter, CMake-picked) | Switching backends requires a rebuild. |

**Default to option 1.** Virtual dispatch is rarely the bottleneck. Choose option 2 only with a measured benchmark in hand (`performance.md`). Choose option 3 when the backend is fundamentally a build-time choice (e.g., the crypto provider — see `worked-examples.md` in `tpm-write-code/references/`).

### A.2 Sketch the port in the domain

Choose the component area before creating files. Growing TPM feature families
use an include folder, source folder, and namespace with the same area name:
`include/tpmkit/<area>/`, `src/domain/<area>/`, and `tpmkit::<area>`.

```
include/tpmkit/pcr/provider.h      <- existing PCR port shape
src/domain/pcr/selection.cpp       <- PCR domain implementation
include/tpmkit/<area>/provider.h   <- new component-family port
src/domain/<area>/*.cpp            <- new component-family domain logic
```

Domain signatures use domain types only — no `openssl/`, no `tss2/`, no OS
headers.

Naming: use the unprefixed concept inside the component namespace. Prefer
`tpmkit::pcr::provider`, `tpmkit::nv::provider`, or
`tpmkit::key::provider`; avoid `_port` and `_interface` suffixes, root-level
prefixed types such as `pcr_provider`, and doubled names such as
`pcr::pcr_provider` (cross-reference: `tpm-write-code` Naming).

For options 2 and 3, the file shape differs:

- **Option 2** — declare a function template constrained on the adapter type. Concept-like SFINAE in C++17 (`std::void_t`, `std::enable_if_t`); document the required member functions.
- **Option 3** — declare the port as a concrete class with no virtual methods. Each adapter `.cpp` provides the symbol bodies; CMake links the chosen one.

### A.3 Write the mock adapter first

Create the mock implementation under `src/adapters/mock/`. If it is a public
testing helper, place the installed header under `include/tpmkit/testing/` and
keep the implementation in `src/adapters/mock/`, matching
`mock_pcr_provider`. The mock is the first user of the port — if the mock is
awkward to write, the port shape is wrong. Iterate on the port before going
further.

For option 1, the mock implements the abstract base. For options 2 and 3, the mock satisfies the same shape (concept members or symbol bodies) — GMock cannot mock these mechanically; write hand-rolled stubs.

### A.4 Write the contract test suite

Create `tests/contract/<port_name>_contract.cpp`. Use GoogleTest parameterized tests (`TEST_P` + `INSTANTIATE_TEST_SUITE_P`) so the same suite runs against every adapter — never copy-paste the suite per backend.

Cover:
- The happy path for each port method.
- Each error category from `error-handling.md` (`input_error`, `security_failure`, `resource_error`, `backend_error`).
- Lifecycle: construction, destruction, move (if applicable).

The contract suite must pass against the mock first. If it does not, the contract is unspecified or the mock is wrong; do not move on.

Cross-reference: `tpm-write-tests` for fixture style, `PrintTo` requirements, and `StrictMock` defaults.

### A.5 Wire CMake

Add the new public header and any `src/domain/<area>/` implementation files to
the appropriate target's `target_sources`. The port belongs to the domain
library target; do not link adapter targets to the domain.

For option 3, define a CMake variable (e.g., `LIB_<PORT>_BACKEND`) and select the adapter source file based on it in the domain library's `target_sources`. See `tpm-build-config` for the project's CMake conventions.

### A.6 Implement the real adapter(s)

Only after the mock and contract suite are in place. Each adapter lives in its own folder under `src/adapters/<backend>/`, with private linkage to its third-party dependency. Translate third-party errors at the boundary (cross-reference: `error-handling.md` Translating third-party errors).

If the adapter is expected to grow, create responsibility subfolders inside
`src/adapters/<backend>/` rather than leaving a flat directory. For the ESYS
adapter, use `context/` for connection and TCTI lifecycle, a component folder
such as `pcr/` for the concrete port implementation and backend/domain
marshalling, and `support/` for adapter-wide helpers such as error translation
and log event names.

Do not implement a growing component by putting every port method and every
helper into one provider translation unit. Scaffold the concrete provider as
command orchestration and add sibling helper files when the first real method
needs them:

```
src/adapters/<backend>/<area>/
├── <backend>_<area>_provider.{h,cpp}  command sequencing and observer callbacks
├── <area>_marshalling.{h,cpp}         backend/domain type conversion
├── <area>_validation.{h,cpp}          pre-dispatch checks and input mapping
└── <area>_logging.{h,cpp}             backend event field construction
```

Add `<area>_sessions`, `<area>_auth`, or `<area>_handles` when those
responsibilities appear. The trigger is responsibility mix, not just line
count; a provider approaching 300 lines is already late.

### A.7 Wire the composition root

Add the selection logic in `src/composition/`. The composition root is the *only* place that names concrete adapter types; everywhere else uses the port type.

For option 1, this is a runtime `std::make_unique<concrete_adapter>(...)` returning a `std::unique_ptr<port>`. For option 3, this is implicit at link time — the composition root names the port directly.

If the new component borrows an existing public owner/context (for example a
service using `tpm_context`'s TPM connection), expose it as a backend-neutral
member factory on that owner instead of a free function named after the
adapter. The member returns the domain port type, uses the owner's logger and
backend state internally, and documents that the returned object must not
outlive the owner.

### A.8 Update consumers

Replace direct uses of the previous concrete type (if any) with the port type, working from the public API inward. Tests that were exercising a specific adapter via a concrete type should now go through the contract suite or the relevant adapter's integration tests (`tests/integration/<backend>/`).

## Workflow B — Adding a new backend adapter for an existing port

Use this workflow when the port already exists and you are adding another implementation behind it (e.g., a new TPM transport, a new key provider).

### B.1 Confirm the port shape fits

Before creating files: read the port header and existing adapters. If the new backend wants to expose a method that no other adapter has, that is a smell — split the port (interface segregation, cross-reference: `tpm-write-code` SOLID applied) rather than adding a "throw not_supported" stub. Stop and revise the port if needed; switch to Workflow A for the split.

### B.2 Create the adapter folder

```
src/adapters/<new_backend>/
├── CMakeLists.txt           target with private link to the third-party library
├── <new_backend>_<port>.h   adapter class, includes only third-party headers it owns
└── <new_backend>_<port>.cpp implementation, error translation
```

For larger adapters, keep the same ownership but split by responsibility:

```
src/adapters/<new_backend>/
├── CMakeLists.txt
├── context/                 backend connection/lifecycle helpers, if any
├── <component>/             concrete provider plus marshalling/validation/logging
└── support/                 backend-wide error/logging helpers
```

Inside `<component>/`, avoid a single provider file that owns every method and
helper. Keep the provider file focused on backend command sequencing. Extract
backend/domain marshalling, pre-dispatch validation, logging field assembly,
authorization/session setup, and resource-handle helpers into separate
component-local modules as soon as those responsibilities appear.

Naming: `<backend>_<port>` shape: `tpm2_fapi_adapter`,
`mock_key_provider`, `software_key_provider`. For component-namespaced ports,
include the component area in the adapter name when it clarifies the backend
role, as `esys_pcr_provider` does for `tpmkit::pcr::provider`. Use the
`_adapter` suffix when the backend name does not already disambiguate.

### B.3 Subclass the port and implement every method

For option 1 ports: subclass the abstract base and implement every pure virtual. For option 2: provide the required members on a concrete type. For option 3: provide the symbol bodies for the new backend's `.cpp`.

Translate third-party errors at the adapter boundary into one of the four domain error categories. Log the original code at the adapter boundary before translating; never propagate raw `TSS2_RC` or OpenSSL error codes into domain types.

### B.4 Run the contract suite against the new adapter

Add an `INSTANTIATE_TEST_SUITE_P(<new_backend>, ...)` line to the existing contract suite. Run it. The new adapter must pass identically — that is what makes adapters interchangeable. A passing contract suite is the entry ticket; without it, the adapter is a bug waiting to happen.

### B.5 Add integration tests

Create `tests/integration/<new_backend>/`. These exercise the *real* backend without mocks (cross-reference: `tpm-write-tests` Integration tests). Cover the same flow set as the contract suite but against the real third-party library.

For TPM backends, integration tests run against `swtpm` in CI. Pin the simulator version alongside the backend's dependency pins in `tpm-build-config`.

### B.6 Wire into the composition root

Add the selection rule (configuration flag, hardware-presence check, capability probe) in `src/composition/`. This is the only place the new concrete type is named.

For runtime selection (option 1), the composition root inspects environment or configuration, instantiates the chosen adapter, and returns it as the port type. For build-time selection (option 3), update the `LIB_<PORT>_BACKEND` switch.

For adapters that are created from an existing context rather than a standalone
composition root, keep the concrete adapter name inside the context adapter
implementation and add a `context.create_*()` member returning the existing
port. Do not expose backend handles or a public backend-named factory just to
construct the adapter.

## Decision tree at a glance

```
Is the port new?
├── Yes → Workflow A
│   ├── Choose polymorphism option (default: 1)
│   ├── Sketch port in domain
│   ├── Mock first
│   ├── Contract suite first
│   ├── Real adapter(s)
│   └── Compose
└── No → Workflow B
    ├── Confirm port shape fits (split if not)
    ├── Adapter folder
    ├── Implement and translate errors
    ├── Contract suite passes (gate)
    ├── Integration tests
    └── Compose
```

## Common mistakes

- **Implementing the real adapter before the mock.** The mock is the cheapest place to discover that the port shape is wrong. Skipping it costs an order of magnitude more rework when the contract reveals a mismatch.
- **Skipping the contract suite for "trivial" adapters.** Adapters that look trivial are exactly where mock/real divergence hides. The contract suite is what makes adapters interchangeable.
- **Growing a provider monolith.** A `*_provider.cpp` that contains every port
  method plus backend/domain marshalling, validation, logging, sessions, and
  resource helpers is already past the useful split point. Extract
  component-local helper modules before adding the next method.
- **Letting a third-party header into a domain file.** Grep for `openssl/` and `tss2/` under `src/domain/` after the change — there should be zero matches. If there are, extract the port more aggressively.
- **Adding a "not supported" stub for a method that does not apply to the new backend.** This is a port-segregation problem in disguise. Split the port.
- **Naming the new adapter after the port shape (`key_provider_v2`, `key_provider_new`).** Name after the backend (`hsm_key_provider`, `pkcs11_key_provider`). The shape lives in the port; the backend lives in the name.
- **Wiring the composition root before the contract suite passes.** A contract failure should be caught in the test tier, not by integration users discovering subtle behavior differences.
- **Exposing a backend-named factory for a context-borrowing adapter.** A public `create_esys_*`-style function turns an internal adapter detail into API. Add a backend-neutral member factory on the owning context instead.

## Error Handling

* **Mock is awkward to write (Workflow A.3).** The port shape is wrong. Iterate the port — narrow it, split it, or rename methods — before going further. The mock is the cheapest place to discover this; do not move on to the real adapter and pay an order of magnitude more rework.
* **Contract suite passes against the mock but fails against the real adapter.** The mock under-specifies the contract. Tighten the mock to match the real adapter's observed behavior (error categories, lifecycle constraints, threading), re-run, and only then proceed.
* **Domain header pulls in `openssl/` or `tss2/` after the change.** Break the chain. Forward declare the type in the domain header, move the third-party include into the adapter `.cpp`, or extract a new port — whichever preserves the dependency-inward rule from `architecture.md`. Do not relax the grep gate in `tpm-build-config` Invariant checks.
* **A new backend wants a method that no other adapter exposes (Workflow B.1).** Interface segregation problem. Stop and split the port — switch to Workflow A for the split. Do not add a "throw not_supported" stub; that is a port-shape bug in disguise.
* **Adapter passes its own integration tests but fails the contract suite.** The adapter is not interchangeable with the mock — that is the bug the contract suite exists to catch. Fix the adapter (or, if the contract is genuinely wrong, fix the contract and re-run against every adapter, including the mock).
* **Composition root names the concrete adapter type outside `src/composition/`.** Move the selection logic into `src/composition/`. Domain and adapter code see the port type only; the composition root is the single place concrete types are named.
* **Need to call back into the old concrete type from a test (Workflow A.8 migration).** Route the test through the contract suite or the adapter's integration tests under `tests/integration/<backend>/`. A test that reaches around the port hides the very divergence the port is meant to surface.

## Cross-references

- `architecture.md` — dependency direction, polymorphism options, FAPI/ESYS-as-internal-detail rule.
- `tpm-write-code` references/`worked-examples.md` — file paths, CMake wiring, and reasoning behind OpenSSL (option 3) and TPM2 TSS (option 1) choices.
- `tpm-write-tests` — contract test framework choice (`TEST_P`/`INSTANTIATE_TEST_SUITE_P`), mock discipline, integration test layout.
- `error-handling.md` — the four error categories and how adapters translate into them.
- `library-api-design.md` — exception-safety and thread-safety contracts that the port must declare.
- `tpm-build-config` — CMake conventions for new targets, vcpkg additions for new third-party deps.
