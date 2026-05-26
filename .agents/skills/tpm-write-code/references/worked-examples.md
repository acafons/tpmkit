# Port-implementation worked examples

Two worked examples illustrating the port options in `.claude/rules/architecture.md` ("How to implement a port"). The decision tables there pick the option; this file shows how the wiring lands in the codebase.

## OpenSSL — option 3 (build-time selection)

- **Port:** `crypto_primitives` — declared in `src/domain/crypto_primitives.h`. Pure C++ types, no `openssl/` includes.
- **Adapter:** `src/adapters/openssl/crypto_primitives_openssl.cpp` — implements the port using `EVP_*` APIs.
- **Wiring:** CMake selects the adapter `.cpp` based on a `LIB_CRYPTO_BACKEND` option. Only one backend compiles per build.
- **Why option 3:** most consumers pick a crypto backend at build time and never swap. The mock for tests can still be a separate `.cpp` selected by the test target — option 3 does not preclude a mock build.

## TPM2 TSS — option 1 (runtime adapter selection)

- **Port:** `pcr::provider` — abstract base class declared in
  `include/tpmkit/pcr/provider.h`, with PCR domain implementation under
  `src/domain/pcr/`. It speaks domain types only — no `TSS2_*` types leak
  through.
- **Adapters, all behind the same port:**
  - `esys_pcr_provider` — ESYS-backed PCR implementation.
  - `mock_pcr_provider` — deterministic public testing helper backed by
    `src/adapters/mock/`.
- **Wiring:** `tpm_context::create_pcr_provider()` creates the adapter from the
  owning context and returns `std::unique_ptr<pcr::provider>`. Selection is not
  exposed to the library consumer — the public API speaks the domain port.
- **FAPI vs. ESYS is *not* a port boundary.** It is an internal adapter detail. Do not create separate ports for FAPI and ESYS — that would leak the TSS layering through the domain.
- **Why option 1:** TPM operations are millisecond-scale (virtual dispatch is free), tests must run without a TPM, and runtime fallback is a real product requirement.

Future TPM component families should reuse the same namespace and folder
pattern. For example, a key-management port should be shaped as
`include/tpmkit/key/provider.h` and `tpmkit::key::provider`, not a root-level
`key_provider` if the key API is expected to grow into a family of types.

## Reading these as templates

When adding a new backend or port, ask which existing example it resembles:

- "One backend per build, no test-time substitution required" → follow the OpenSSL shape.
- "Mock for tests, runtime fallback, or hardware presence detection" → follow the TPM2 TSS shape.

If neither fits, revisit the decision table in `architecture.md` before committing to a layout.
