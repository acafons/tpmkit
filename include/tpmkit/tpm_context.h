#pragma once

/**
 * @file tpmkit/tpm_context.h
 * @brief Public TPM connection abstraction and configuration types.
 *
 * Declares the backend-neutral TPM context surface and the configuration
 * carrier used to select a TCTI source and startup behavior.
 */

#include <tpmkit/api.h>
#include <tpmkit/logger.h>
#include <tpmkit/result.h>

#include <memory>
#include <string>
#include <variant>

extern "C" {
/**
 * @brief Opaque TPM2 TSS TCTI context type accepted by owned-handle configs.
 *
 * tpmkit only stores and transfers ownership of this forward-declared C type
 * at the public boundary; implementation files include the full TSS headers.
 *
 * @thread_safety Follows the TPM2 TSS TCTI implementation backing the handle.
 * @exception_safety No operations are exposed by this forward declaration.
 */
typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;
}

namespace tpmkit {

/**
 * @brief TCTI source selected by a tpm2-tools-compatible configuration string.
 *
 * @note Error categories: invalid or empty configuration is reported by
 *       tpm_context::create as error_category::input_error.
 * @thread_safety Thread-compatible.
 * @exception_safety Default construction and moves provide the same guarantee
 * as `std::string`.
 * @since v0.1
 */
struct tcti_string_config {
    /** @brief TCTI configuration string validated by tpm_context::create. */
    std::string config;
};

/**
 * @brief TCTI source that transfers ownership of an existing TSS handle.
 *
 * @note The deleter owns the finalization policy and must be valid for the
 *       handle it receives.
 * @note Error categories: null or unusable handles are reported by
 *       tpm_context::create as error_category::input_error or
 *       error_category::backend_error depending on the failure point.
 * @thread_safety Thread-compatible.
 * @exception_safety Move operations are noexcept when the stored
 * `std::unique_ptr` move is noexcept.
 * @since v0.1
 */
struct tcti_owned_handle {
    /** @brief Owned opaque TCTI context and finalizer consumed by tpm_context::create. */
    std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)> handle;
};

/**
 * @brief Configuration used to atomically create a tpm_context.
 *
 * @note Error categories: tpm_context::create may return input_error,
 *       resource_error, or backend_error while consuming this configuration.
 * @thread_safety Thread-compatible.
 * @exception_safety This aggregate follows the guarantees of its member types.
 * @see tpm_context
 * @since v0.1
 */
struct tpm_context_config {
    /**
     * @brief TPM startup behavior requested during context creation.
     *
     * @thread_safety Enum values are immutable and thread-safe.
     * @exception_safety No operations throw.
     * @since v0.1
     */
    enum class startup_mode {
        /** @brief Invoke clear-state TPM startup. */
        clear,
        /** @brief Invoke state-preserving TPM startup. */
        state,
        /** @brief Skip TPM startup; the caller owns startup readiness. */
        skip,
    };

    /** @brief Explicit TCTI source; defaults to an empty string config that create rejects. */
    std::variant<tcti_string_config, tcti_owned_handle> tcti;

    /** @brief Requested startup behavior; defaults to startup_mode::clear. */
    startup_mode startup = startup_mode::clear;

    /** @brief Logger port used for lifecycle records; null selects the library default. */
    std::shared_ptr<logger> log;
};

/**
 * @brief Backend-neutral, move-only TPM connection handle.
 *
 * tpm_context owns the implementation-specific TPM connection state behind a
 * Pimpl so public consumers do not include or link against TSS headers through
 * this header.
 *
 * @note Construction is atomic through create(tpm_context_config); callers
 *       never observe a half-initialized context.
 * @thread_safety Thread-compatible. Independent instances may be used
 *       concurrently; a shared instance requires external synchronization.
 * @exception_safety Destructor and moves are noexcept; create provides the
 * strong guarantee on failure.
 * @since v0.1
 */
class TPMKIT_API tpm_context final {
public:
    /**
     * @brief Destroy the TPM context and release backend resources.
     *
     * @thread_safety Single-threaded for the destroyed instance.
     * @exception_safety noexcept.
     */
    ~tpm_context() noexcept;

    /**
     * @brief Copy construction is disabled because the context owns TPM state.
     *
     * @param[in] other Source context; copying is intentionally unavailable.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    tpm_context(const tpm_context& other) = delete;

    /**
     * @brief Copy assignment is disabled because the context owns TPM state.
     *
     * @param[in] other Source context; copying is intentionally unavailable.
     * @return Reference to this context if the operation existed.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    tpm_context& operator=(const tpm_context& other) = delete;

    /**
     * @brief Move-construct a TPM context, transferring backend ownership.
     *
     * @param[in] other Source context. Valid but unspecified after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    tpm_context(tpm_context&& other) noexcept;

    /**
     * @brief Move-assign a TPM context, replacing current backend ownership.
     *
     * @param[in] other Source context. Valid but unspecified after the move.
     * @return Reference to this context.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    tpm_context& operator=(tpm_context&& other) noexcept;

    /**
     * @brief Atomically create a TPM context from explicit configuration.
     *
     * @param[in] config TCTI source, startup mode, and optional logger. The
     *                   configuration is consumed by value.
     * @return On success, returns the fully initialized context. On failure,
     *         returns `error` with category in {error_category::input_error,
     *         error_category::resource_error, error_category::backend_error}.
     * @thread_safety Thread-safe.
     * @exception_safety Strong; failed creation does not publish a partially
     * initialized context.
     */
    [[nodiscard]] static outcome<tpm_context> create(tpm_context_config config);

    /**
     * @brief Return the opaque in-library ESYS handle.
     *
     * @return Non-owning backend handle for in-library adapters.
     * @note This is not a consumer extension point.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] void* esys_handle() const noexcept;

private:
    class impl;

    explicit tpm_context(std::unique_ptr<impl> implementation) noexcept;

    std::unique_ptr<impl> impl_;
};

} // namespace tpmkit
