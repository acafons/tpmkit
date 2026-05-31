#pragma once

/**
 * @file tpmkit/tpm2_esys/owned_tcti_context.h
 * @brief Low-level ESYS context construction from an owned TPM2 TSS TCTI handle.
 *
 * This header is intentionally separate from the backend-neutral
 * `<tpmkit/tpm_context.h>` API. Include it only when a caller deliberately
 * owns a TPM2 TSS TCTI context and wants tpmkit to consume that handle.
 */

#include <tpmkit/logging/logger.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <memory>

extern "C" {
/**
 * @brief Opaque TPM2 TSS TCTI context type accepted by the low-level ESYS API.
 *
 * The concrete ABI layout is owned by TPM2 TSS. This header forward-declares
 * the type so callers can transfer ownership without including TSS headers
 * through the backend-neutral tpmkit API.
 *
 * @thread_safety Follows the TPM2 TSS TCTI implementation backing the handle.
 * @exception_safety No operations are exposed by this forward declaration.
 */
typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;
}

namespace tpmkit::tpm2_esys {

/**
 * @brief Owned TPM2 TSS TCTI handle consumed by the low-level ESYS factory.
 *
 * @note The deleter owns the finalization policy and must be valid for the
 * handle it receives.
 * @thread_safety Thread-compatible.
 * @exception_safety Move operations are noexcept when the stored
 * `std::unique_ptr` move is noexcept.
 * @since v0.1
 */
struct owned_tcti_context {
    /** @brief Owned opaque TCTI context and finalizer consumed by create_context(). */
    std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)> handle{nullptr, nullptr};
};

/**
 * @brief Create a tpm_context from an already-created TPM2 TSS TCTI context.
 *
 * This is the explicit low-level ESYS/TSS entry point. Backend-neutral callers
 * should use `tpmkit::tpm_context::create(tpm_context_config)` with a string
 * TCTI configuration instead.
 *
 * @param[in] tcti Owned TCTI handle and deleter. The handle must not be null
 * and the deleter must be valid for the handle.
 * @param[in] startup Requested TPM startup behavior.
 * @param[in] log Logger port used for lifecycle records; null selects
 * `noop_logger`.
 * @return On success, returns the fully initialized context. On failure,
 * returns `error` with category in {error_category::input_error,
 * error_category::resource_error, error_category::backend_error}.
 * @thread_safety Thread-safe.
 * @exception_safety Strong; failed creation does not publish a partially
 * initialized context.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API outcome<tpm_context>
create_context(owned_tcti_context tcti,
               tpm_context_config::startup_mode startup = tpm_context_config::startup_mode::clear,
               std::shared_ptr<logger> log = nullptr);

} // namespace tpmkit::tpm2_esys
