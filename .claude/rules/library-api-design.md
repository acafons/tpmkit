# C++17 Library API Design

These rules govern the public surface of the library. They apply to installed/public headers and exported symbols only — internal code follows `code-standards.md`.

## Public surface

- Public headers expose only the API surface — never implementation details, private helpers, or third-party types that are not part of the contract.
- Place public headers under `include/<library_name>/` and internal headers under `src/`. Only `include/` is installed.
- Keep public headers free of third-party includes when possible. Forward declare and move the include into the `.cpp` file.
- Backend-neutral public headers must not expose backend handles or ABI types
  such as `TSS2_TCTI_CONTEXT`, `ESYS_CONTEXT`, OpenSSL handles, or spdlog
  concrete types. Only add a backend-named low-level public API when a concrete
  use case cannot be represented by domain ports or backend-neutral
  configuration. The current approved shape is one-way adoption of an owned TPM2
  TSS TCTI handle through `<tpmkit/tpm2_esys/...>`, and that header must not be
  included from backend-neutral headers.
- Do not write `using namespace` at namespace scope in any public header.
- Wrap the public API in a single top-level namespace named after the library.
- Do not place implementation-only types in public headers. When a helper must
  appear in a public header for C++ mechanics such as templates, inline
  functions, ADL, traits, or narrowly scoped friend declarations, put it under a
  nested `detail` namespace and document that `detail` is not part of the public
  contract.
- Cohesive public component families live in nested domain namespaces that
  match their include folder. For example, PCR headers live under
  `<tpmkit/pcr/...>` and expose `tpmkit::pcr::*`. Future NV and key-management
  APIs should use the same shape, such as `<tpmkit/nv/...>` with `tpmkit::nv`
  and `<tpmkit/key/...>` with `tpmkit::key`.
- Inside a nested domain namespace, type names are the domain concept without
  repeating the namespace prefix. Prefer `tpmkit::pcr::provider`,
  `tpmkit::pcr::selection`, and `tpmkit::pcr::index`; avoid root-level
  prefixed names such as `tpmkit::pcr_provider`, and avoid doubled names such
  as `tpmkit::pcr::pcr_selection`.
- Do not add root aliases for nested component types unless an explicit
  compatibility plan requires them. New breaking APIs should document the new
  namespace directly instead of carrying permanent aliases.

## Context-derived factories

- A component that borrows resources from an existing public owner/context is
  created by a backend-neutral member factory on that owner, such as
  `ctx.create_pcr_provider()`.
- Public context-derived factories return domain port types or value types, not
  adapter-specific concrete types, and their names do not expose backend names
  such as ESYS or FAPI.
- Do not expose raw backend handles, public `detail` accessors, or friend
  functions solely to wire another in-library component. The owning context's
  implementation performs that wiring internally.
- The factory documentation states the borrowed lifetime rule: returned objects
  must not outlive the owner/context or any non-owning collaborator passed to
  the factory.

## Pimpl idiom

- Public classes with non-trivial internals use Pimpl. It insulates layout changes from consumers (cross-reference: ABI section below) and keeps adapter headers out of the public surface.
- Pimpl classes are move-only by default. Provide copy only when value semantics genuinely make sense, and implement it as a deep copy.
- Do not apply Pimpl to internal types or to small value types — the heap allocation and indirection are not justified.

For the C++17 mechanics (forward declaration, where the destructor must be defined, deep-copy implementation, common mistakes, and migrating an existing class), see `.agents/skills/tpm-write-code/references/pimpl-mechanics.md`.

## ABI and symbol visibility

- Default visibility is hidden. Annotate exported symbols with a library-specific export macro (e.g., `LIBNAME_API`) that resolves to the platform's visibility attribute.
- Do not export class templates or inline functions as part of the ABI — they are instantiated in the consumer's translation unit.
- Treat changes to public class layout, virtual tables, or inline definitions as ABI-breaking. Pimpl insulates layout changes; prefer it for any class likely to evolve.
- Do not expose standard-library types across the ABI when binary compatibility across compiler versions matters (e.g., `std::string` layout differs across standard libraries).

## Versioning and deprecation

- Follow Semantic Versioning. Bump major on any breaking change to the public API or ABI.
- Mark deprecated APIs with `[[deprecated("use X instead")]]` and keep them for at least one minor cycle before removal.
- Document the minimum supported compiler and standard-library versions in the project README.

## Exception safety

- Document the exception guarantee for every public function: `noexcept`, strong, basic, or none.
- Public destructors and move operations must be `noexcept`.
- Do not let exceptions cross a `extern "C"` boundary; catch and translate to error codes at the boundary.

## Thread safety

- Document the thread-safety contract for every public type: thread-compatible (independent instances are safe), thread-safe (shared instances are safe), or single-threaded.
- Default to thread-compatible unless the type is explicitly designed for shared use.
- Free functions on the public API are thread-safe unless documented otherwise.

## Headers, build, and packaging

- Decide header-only vs. compiled per component, not per project. Prefer compiled for anything non-trivial — header-only inflates compile times for consumers.
- Provide a CMake package config (`<library>Config.cmake`) so consumers can `find_package` the library.
- Export an `INTERFACE` target that propagates only the public include directory and required compile features (e.g., `cxx_std_17`).
