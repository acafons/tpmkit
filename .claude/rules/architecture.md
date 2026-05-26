# C++17 Architecture

This project follows **hexagonal architecture** (ports and adapters). The goal is to keep the domain core free of third-party dependencies so backends like OpenSSL and the TPM2 TSS stack (ESYS, FAPI) can be swapped, mocked, or omitted without touching domain code.

## Layers

- **Domain** — pure C++17, the library's core abstractions and use cases. No third-party includes. No I/O. Speaks only in domain types.
- **Ports** — abstract interfaces (or concept-constrained templates) declared by the domain to express what it needs from the outside world. Owned by the domain layer.
- **Adapters** — concrete implementations of ports that depend on third-party libraries (OpenSSL, TPM2 TSS, the OS). Owned by the adapter layer.
- **Composition root** — the single place that wires concrete adapters into the domain. Lives in the entry point of each consumer (binary, test, or example).

## Dependency direction

- Dependencies always point **inward**: adapters depend on the domain, never the reverse.
- Domain headers must not include any third-party header. Grep should find zero matches for `openssl/`, `tss2/`, etc. under the domain folder.
- Ports are declared in the domain. Adapters live in their own translation units and are linked in by the composition root.
- The public library API (see `library-api-design.md`) is part of the domain layer. Consumers see domain types, not adapter types.
- Public context-owned services that borrow backend state are exposed as
  backend-neutral member factories returning domain ports. The concrete adapter
  is named only inside the owning context's adapter implementation.

## Folder layout

```
include/<library>/        public API (domain types and ports)
src/domain/               domain logic and port definitions
src/adapters/openssl/     OpenSSL adapter
src/adapters/tpm2_fapi/   FAPI adapter
src/adapters/tpm2_esys/   ESYS adapter
src/adapters/logging/<backend>/  logging adapter backends
src/adapters/mock/        in-memory adapter for tests
src/composition/          factories that pick and wire adapters
tests/unit/public_api/    installed API behavior and public-header smoke tests
tests/unit/domain/        unit tests against the domain with mock adapters
tests/unit/testing/       public test-helper tests
tests/unit/testing/<adapter>/  adapter-shaped public test-helper tests
tests/unit/<adapter>/          isolated adapter unit tests with no live backend
tests/unit/logging/<backend>/  isolated logging adapter unit tests
tests/integration/<adapter>/   real adapter tests against the backend
```

Build manifests mirror this layout. The top-level `CMakeLists.txt` stays an
orchestrator for project setup, cross-cutting CMake modules, and
`add_subdirectory` calls. Target definitions, source lists, include paths, and
adapter dependencies live in the closest meaningful module `CMakeLists.txt`
rather than accumulating in the repository root.

## How to implement a port

C++17 gives three viable options. Pick per port, not per project.

1. **Runtime polymorphism** — abstract base class with virtual methods. Allows runtime swap, mocking, and fallback. Costs a virtual dispatch per call.
2. **Compile-time polymorphism** — template parameter with a concept (or SFINAE in C++17). Zero overhead, but bakes the choice in at compile time and pulls adapter headers into domain headers.
3. **Build-time selection** — one header declaring the port, one `.cpp` per adapter, picked by CMake. Zero overhead, runtime-fixed, no header pollution.

**Choosing between them:**

| Need | Option |
|---|---|
| Mock for tests, runtime fallback, or hardware presence detection | 1 (runtime) |
| Hot path where virtual dispatch is measurable | 2 (compile-time) |
| One backend per build, no test-time substitution required | 3 (build-time) |

When in doubt, use option 1. Virtual dispatch is rarely the bottleneck, and the testability win pays for itself.

## Backend choices for this project

The two main backends are settled:

- **OpenSSL** uses option 3 (build-time selection). CMake picks one crypto backend per build via `LIB_CRYPTO_BACKEND`.
- **TPM2 TSS** uses option 1 (runtime adapter selection). All TPM adapters — FAPI, ESYS, software fallback, mock — sit behind a single `key_provider` port and are picked by the composition root at runtime. The **FAPI/ESYS split is an internal adapter detail, not a port boundary** — never declare separate ports for them.
- ESYS/FAPI details do not appear in public factory names, public context
  accessors, or public headers unless the project deliberately introduces a
  separate low-level API. Prefer `tpm_context::create_*` factories that return
  domain ports for components borrowing a TPM connection.

For the full walkthrough — file paths, CMake wiring, the per-adapter list, and the reasoning behind each choice — see `.claude/skills/tpm-write-code/references/worked-examples.md`.

## Rules for third-party dependencies

- Every new third-party dependency must enter through an adapter. Never include third-party headers from domain code.
- Each adapter lives in its own folder under `src/adapters/<name>/` and links its third-party dependency privately. Adapter families with multiple implementations of one port may group by port first, as logging does under `src/adapters/logging/<backend>/`. Consumers of the library do not transitively depend on adapter libraries.
- If a third-party type must appear in a domain signature (rare), wrap it in a domain type first.
- Adapters translate third-party errors into domain errors at the boundary. Never let `TSS2_RC`, `unsigned long` OpenSSL error codes, or library-specific exceptions cross into the domain.

## Testing implications

- Public API tests live under `tests/unit/public_api/` and cover installed headers, value types, configuration objects, and public API contracts.
- Domain tests use mock adapters and run with no third-party dependencies linked.
- Public testing-helper tests live under `tests/unit/testing/`; helper tests tied to an adapter or third-party ABI live under `tests/unit/testing/<adapter>/`.
- Adapter-internal unit tests live under `tests/unit/<adapter>/`; grouped adapter families mirror their source layout, as logging does under `tests/unit/logging/<backend>/`. They may compile against backend SDK headers or constants, but they do not open real backend resources.
- Each adapter has its own integration tests that exercise the real third-party library.
- The mock adapter for a given port lives next to the real adapters under `src/adapters/mock/` so the contract is tested uniformly.
