#pragma once

/**
 * @file tpmkit/pcr/provider.h
 * @brief Domain port for TPM PCR operations.
 */

#include <tpmkit/api.h>
#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/bank.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/result_types.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>
#include <tpmkit/secret_buffer.h>

#include <gsl/span>

#include <cstdint>

namespace tpmkit::pcr {

/**
 * @brief Abstract port for TPM PCR operations.
 *
 * Implementations execute PCR commands through a concrete backend while this
 * port keeps consumers and domain logic independent of TPM2 TSS types.
 *
 * @thread_safety Thread-compatible. Implementations document stronger
 * guarantees when shared instances are safe.
 * @exception_safety Destructor is noexcept. Operation failures are returned as
 * `outcome<T>` values; implementations document any stronger guarantees.
 * @since v0.1
 */
class TPMKIT_API provider {
public:
    /**
     * @brief Destroy the PCR provider adapter.
     *
     * @thread_safety Single-threaded for the destroyed adapter unless the
     * adapter documents stronger behavior.
     * @exception_safety noexcept.
     */
    virtual ~provider() noexcept = default;

    /**
     * @brief Read PCR values for one selection.
     *
     * @param[in] selection PCR bank and indices to read.
     * @return Read result on success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<read_result> read(const selection& selection) = 0;

    /**
     * @brief Extend one PCR with caller-provided digest values.
     *
     * @param[in] index PCR index to extend.
     * @param[in] digests Digest values for explicit PCR banks.
     * @return Empty success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<void> extend(index index,
                                               gsl::span<const digest_value> digests) = 0;

    /**
     * @brief Extend one PCR with raw event data digested by the TPM.
     *
     * @param[in] index PCR index to extend.
     * @param[in] event_data Raw event bytes.
     * @return Event result on success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<event_result> event(index index,
                                                      gsl::span<const std::uint8_t> event_data) = 0;

    /**
     * @brief Reset one PCR to its TPM-defined initial value.
     *
     * @param[in] index PCR index to reset.
     * @return Empty success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<void> reset(index index) = 0;

    /**
     * @brief Allocate active PCR banks.
     *
     * @param[in] banks Requested PCR banks.
     * @return Allocation result on success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<allocate_result> allocate(gsl::span<const bank> banks) = 0;

    /**
     * @brief Set an authorization value on one PCR.
     *
     * @param[in] index PCR index to protect.
     * @param[in] auth Authorization value to transfer to the backend.
     * @return Empty success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<void> set_auth_value(index index, secret_buffer auth) = 0;

    /**
     * @brief Set an authorization policy on one PCR.
     *
     * @param[in] index PCR index to protect.
     * @param[in] policy_alg Hash algorithm used for the policy digest.
     * @param[in] policy_digest Policy digest bytes.
     * @return Empty success, or a domain error on expected failure.
     */
    [[nodiscard]] virtual outcome<void>
    set_auth_policy(index index, hash_algorithm policy_alg,
                    gsl::span<const std::uint8_t> policy_digest) = 0;
};

} // namespace tpmkit::pcr
