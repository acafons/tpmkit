# Port-implementation worked examples

Two worked examples illustrating the port options in `.claude/rules/architecture.md` ("How to implement a port"). The decision tables there pick the option; this file shows how the wiring lands in the codebase.

## OpenSSL — option 3 (build-time selection)

- **Port:** `crypto_primitives` — declared in `src/domain/crypto_primitives.h`. Pure C++ types, no `openssl/` includes.
- **Adapter:** `src/adapters/openssl/crypto_primitives_openssl.cpp` — implements the port using `EVP_*` APIs.
- **Wiring:** CMake selects the adapter `.cpp` based on a `LIB_CRYPTO_BACKEND` option. Only one backend compiles per build.
- **Why option 3:** most consumers pick a crypto backend at build time and never swap. The mock for tests can still be a separate `.cpp` selected by the test target — option 3 does not preclude a mock build.

## TPM2 TSS — option 1 (runtime adapter selection)

- **Port:** `key_provider` — abstract base class declared in `src/domain/key_provider.h`. Methods like `create_signing_key`, `sign`, `load_persistent_key`. Speaks domain types only — no `TSS2_*` types leak through.
- **Adapters, all behind the same port:**
  - `tpm2_fapi_adapter` — default, high-level, covers most operations with less code.
  - `tpm2_esys_adapter` — fine-grained session and policy control for operations FAPI does not expose.
  - `software_key_provider` — pure software fallback (uses the OpenSSL adapter internally) for hardware-absent environments.
  - `mock_key_provider` — in-memory, deterministic, used by every domain unit test.
- **Wiring:** the composition root picks the adapter at runtime based on configuration and hardware presence. Selection is not exposed to the library consumer — the public API speaks `key_provider`.
- **FAPI vs. ESYS is *not* a port boundary.** It is an internal adapter detail. Do not create separate ports for FAPI and ESYS — that would leak the TSS layering through the domain.
- **Why option 1:** TPM operations are millisecond-scale (virtual dispatch is free), tests must run without a TPM, and runtime fallback is a real product requirement.

## Reading these as templates

When adding a new backend or port, ask which existing example it resembles:

- "One backend per build, no test-time substitution required" → follow the OpenSSL shape.
- "Mock for tests, runtime fallback, or hardware presence detection" → follow the TPM2 TSS shape.

If neither fits, revisit the decision table in `architecture.md` before committing to a layout.
