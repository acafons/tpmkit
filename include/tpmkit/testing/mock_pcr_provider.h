#pragma once

/**
 * @file tpmkit/testing/mock_pcr_provider.h
 * @brief Programmable PCR provider test double.
 *
 * Declares the PCR provider mock shipped by the `tpmkit_testing` target.
 */

#include <tpmkit/pcr/provider.h>
#include <tpmkit/testing/testing_api.h>

#include <cstddef>
#include <cstdint>

/**
 * @namespace tpmkit::testing
 * @brief Public test substitutes and adapters for tpmkit consumers.
 *
 * @warning Experimental - API may change before 1.0.0.
 */
namespace tpmkit::testing {

/**
 * @brief Programmable PCR provider test double.
 *
 * mock_pcr_provider implements all PCR provider operations with caller-supplied
 * outcomes and call counters so consumer tests can exercise PCR logic without a
 * TPM backend.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible. Shared instances require external
 * synchronization.
 * @exception_safety Operation methods follow `outcome` copy guarantees.
 * @see pcr::provider
 * @since v0.1
 */
class TPMKIT_TESTING_API mock_pcr_provider final : public pcr::provider {
public:
    /**
     * @brief Construct a provider whose operations fail with backend_error by default.
     *
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    mock_pcr_provider();

    /**
     * @brief Return the programmed allocation outcome and record one call.
     *
     * @param[in] banks Requested PCR banks.
     * @return Programmed allocation outcome.
     */
    [[nodiscard]] outcome<pcr::allocate_result> allocate(gsl::span<const pcr::bank> banks) final;

    /**
     * @brief Return how often allocate() has been called.
     *
     * @return Allocate call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t allocate_call_count() const noexcept;

    /**
     * @brief Reset every operation call counter to zero.
     *
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    void clear_call_counts() noexcept;

    /**
     * @brief Return the programmed event outcome and record one call.
     *
     * @param[in] index PCR index to extend.
     * @param[in] event_data Raw event bytes.
     * @return Programmed event outcome.
     */
    [[nodiscard]] outcome<pcr::event_result> event(pcr::index index,
                                                   gsl::span<const std::uint8_t> event_data) final;

    /**
     * @brief Return how often event() has been called.
     *
     * @return Event call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t event_call_count() const noexcept;

    /**
     * @brief Return the programmed extend outcome and record one call.
     *
     * @param[in] index PCR index to extend.
     * @param[in] digests Digest values for explicit PCR banks.
     * @return Programmed extend outcome.
     */
    [[nodiscard]] outcome<void> extend(pcr::index index,
                                       gsl::span<const pcr::digest_value> digests) final;

    /**
     * @brief Return how often extend() has been called.
     *
     * @return Extend call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t extend_call_count() const noexcept;

    /**
     * @brief Return the programmed read outcome and record one call.
     *
     * @param[in] selection PCR selection to read.
     * @return Programmed read outcome.
     */
    [[nodiscard]] outcome<pcr::read_result> read(const pcr::selection& selection) final;

    /**
     * @brief Return how often read() has been called.
     *
     * @return Read call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t read_call_count() const noexcept;

    /**
     * @brief Return the programmed reset outcome and record one call.
     *
     * @param[in] index PCR index to reset.
     * @return Programmed reset outcome.
     */
    [[nodiscard]] outcome<void> reset(pcr::index index) final;

    /**
     * @brief Return how often reset() has been called.
     *
     * @return Reset call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t reset_call_count() const noexcept;

    /**
     * @brief Program the outcome returned by allocate().
     *
     * @param[in] result Allocation outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_allocate_result(outcome<pcr::allocate_result> result);

    /**
     * @brief Return the programmed SetAuthPolicy outcome and record one call.
     *
     * @param[in] index PCR index to protect.
     * @param[in] policy_alg Hash algorithm used for the policy digest.
     * @param[in] policy_digest Policy digest bytes.
     * @return Programmed SetAuthPolicy outcome.
     */
    [[nodiscard]] outcome<void> set_auth_policy(pcr::index index, hash_algorithm policy_alg,
                                                gsl::span<const std::uint8_t> policy_digest) final;

    /**
     * @brief Return how often set_auth_policy() has been called.
     *
     * @return SetAuthPolicy call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t set_auth_policy_call_count() const noexcept;

    /**
     * @brief Program the outcome returned by set_auth_policy().
     *
     * @param[in] result SetAuthPolicy outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_auth_policy_result(outcome<void> result);

    /**
     * @brief Return the programmed SetAuthValue outcome and record one call.
     *
     * @param[in] index PCR index to protect.
     * @param[in] auth Authorization value to transfer to the backend.
     * @return Programmed SetAuthValue outcome.
     */
    [[nodiscard]] outcome<void> set_auth_value(pcr::index index, secret_buffer auth) final;

    /**
     * @brief Return how often set_auth_value() has been called.
     *
     * @return SetAuthValue call count.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t set_auth_value_call_count() const noexcept;

    /**
     * @brief Program the outcome returned by set_auth_value().
     *
     * @param[in] result SetAuthValue outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_auth_value_result(outcome<void> result);

    /**
     * @brief Program the outcome returned by event().
     *
     * @param[in] result Event outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_event_result(outcome<pcr::event_result> result);

    /**
     * @brief Program the outcome returned by extend().
     *
     * @param[in] result Extend outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_extend_result(outcome<void> result);

    /**
     * @brief Program the outcome returned by read().
     *
     * @param[in] result Read outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_read_result(outcome<pcr::read_result> result);

    /**
     * @brief Program the outcome returned by reset().
     *
     * @param[in] result Reset outcome to copy into this provider.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong.
     */
    void set_reset_result(outcome<void> result);

private:
    std::size_t allocate_call_count_{0U};
    outcome<pcr::allocate_result> allocate_result_;
    std::size_t event_call_count_{0U};
    outcome<pcr::event_result> event_result_;
    std::size_t extend_call_count_{0U};
    outcome<void> extend_result_;
    std::size_t read_call_count_{0U};
    outcome<pcr::read_result> read_result_;
    std::size_t reset_call_count_{0U};
    outcome<void> reset_result_;
    std::size_t set_auth_policy_call_count_{0U};
    outcome<void> set_auth_policy_result_;
    std::size_t set_auth_value_call_count_{0U};
    outcome<void> set_auth_value_result_;
};

} // namespace tpmkit::testing
