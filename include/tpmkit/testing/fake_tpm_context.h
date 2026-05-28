#pragma once

/**
 * @file tpmkit/testing/fake_tpm_context.h
 * @brief Passive test substitute for tpmkit::tpm_context.
 *
 * Declares a lifecycle-only fake context with the same public method shape as
 * tpm_context for construction, move semantics, destruction, PCR provider
 * creation, and configuration introspection.
 */

#include <tpmkit/testing/testing_api.h>
#include <tpmkit/tpm_context.h>

#include <memory>
#include <string>

/**
 * @namespace tpmkit::testing
 * @brief Public test substitutes and adapters for tpmkit consumers.
 *
 * @warning Experimental - API may change before 1.0.0.
 */
namespace tpmkit::testing {

/**
 * @brief Passive, move-only TPM context substitute for unit tests.
 *
 * fake_tpm_context stores the configuration it was created with and performs
 * the same construction-time validation that does not require touching TSS. It
 * does not open a TCTI, initialize a TPM backend, start a TPM, or acquire
 * system resources.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible. Independent instances may be used
 *       concurrently; a shared instance requires external synchronization.
 * @exception_safety Destructor and moves are noexcept; create provides the
 * strong guarantee on validation failure.
 * @see tpmkit::tpm_context
 * @since v0.1
 */
class TPMKIT_TESTING_API fake_tpm_context final {
public:
    /**
     * @brief Destroy the fake context.
     *
     * @thread_safety Single-threaded for the destroyed instance.
     * @exception_safety noexcept.
     */
    ~fake_tpm_context() noexcept;

    /**
     * @brief Copy construction is disabled to mirror tpm_context ownership.
     *
     * @param[in] other Source context; copying is intentionally unavailable.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    fake_tpm_context(const fake_tpm_context& other) = delete;

    /**
     * @brief Copy assignment is disabled to mirror tpm_context ownership.
     *
     * @param[in] other Source context; copying is intentionally unavailable.
     * @return Reference to this context if the operation existed.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    fake_tpm_context& operator=(const fake_tpm_context& other) = delete;

    /**
     * @brief Move-construct a fake context.
     *
     * @param[in] other Source context. Valid but unspecified after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    fake_tpm_context(fake_tpm_context&& other) noexcept;

    /**
     * @brief Move-assign a fake context.
     *
     * @param[in] other Source context. Valid but unspecified after the move.
     * @return Reference to this context.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    fake_tpm_context& operator=(fake_tpm_context&& other) noexcept;

    /**
     * @brief Create a fake context from explicit configuration.
     *
     * @param[in] config TCTI source, startup mode, and optional logger. The
     *                   configuration is consumed by value and stored for
     *                   read-only introspection.
     * @return On success, returns the constructed fake context. On failure,
     *         returns `error` with category in {error_category::input_error}.
     * @thread_safety Thread-safe.
     * @exception_safety Strong; failed creation does not publish an instance.
     */
    [[nodiscard]] static outcome<fake_tpm_context> create(tpm_context_config config);

    /**
     * @brief Create a fake context from a string TCTI configuration.
     *
     * This overload mirrors tpmkit::tpm_context::create(std::string,
     * tpm_context_config::startup_mode, std::shared_ptr<logger>) and stores the
     * equivalent `tpm_context_config` for introspection.
     *
     * @param[in] tcti_config Explicit tpm2-tools-compatible TCTI string. It
     *                        must be non-empty and use the `name:args` shape.
     * @param[in] startup Requested TPM startup behavior to record.
     * @param[in] log Logger port to store in the consumed configuration.
     * @return On success, returns the constructed fake context. On failure,
     *         returns `error` with category in {error_category::input_error}.
     * @thread_safety Thread-safe.
     * @exception_safety Strong; failed creation does not publish an instance.
     * @since v0.1
     */
    [[nodiscard]] static outcome<fake_tpm_context>
    create(std::string tcti_config,
           tpm_context_config::startup_mode startup = tpm_context_config::startup_mode::clear,
           std::shared_ptr<logger> log = nullptr);

    /**
     * @brief Return the configuration consumed by create().
     *
     * @return Const reference valid for this fake context instance lifetime.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] const tpm_context_config& last_config() const noexcept;

    /**
     * @brief Attempt to create a PCR provider from the fake context.
     *
     * @param[in] observer Optional non-owning PCR observer. It is ignored.
     * @return Always `resource_error` because this fake has no TPM backend
     *         connection to borrow.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; no state is modified.
     * @since v0.1
     */
    [[nodiscard]] outcome<std::unique_ptr<pcr::provider>>
    create_pcr_provider(pcr::observer* observer = nullptr);

private:
    explicit fake_tpm_context(tpm_context_config config) noexcept;

    tpm_context_config config_;
};

} // namespace tpmkit::testing
