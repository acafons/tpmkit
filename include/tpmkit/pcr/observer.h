#pragma once

/**
 * @file tpmkit/pcr/observer.h
 * @brief Optional no-throw observer port for PCR measurement events.
 */

#include <tpmkit/api.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/result_types.h>

#include <gsl/span>

#include <cstdint>

namespace tpmkit {

/**
 * @brief Observer notified after successful PCR measurement operations.
 *
 * Implementations may record an event log or perform other side effects, but
 * must never propagate failures to the PCR operation that invoked them.
 *
 * @thread_safety Thread-compatible. Implementations document stronger
 * guarantees when shared instances are safe.
 * @exception_safety Destructor and observer callbacks are noexcept.
 * @since v0.1
 */
class TPMKIT_API pcr_observer {
public:
    /**
     * @brief Destroy the PCR observer adapter.
     *
     * @thread_safety Single-threaded for the destroyed adapter unless the
     * adapter documents stronger behavior.
     * @exception_safety noexcept.
     */
    virtual ~pcr_observer() noexcept = default;

    /**
     * @brief Observe a successful PCR extend operation.
     *
     * @param[in] index PCR index that was extended.
     * @param[in] digests Digest values supplied to the extend operation.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept; implementations swallow observer failures.
     */
    virtual void on_extend(pcr_index index, gsl::span<const pcr_digest_value> digests) noexcept = 0;

    /**
     * @brief Observe a successful PCR event operation.
     *
     * @param[in] index PCR index that was extended.
     * @param[in] event_data Raw event bytes supplied to the TPM.
     * @param[in] result Digest values returned by the event operation.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept; implementations swallow observer failures.
     */
    virtual void on_event(pcr_index index, gsl::span<const std::uint8_t> event_data,
                          const pcr_event_result& result) noexcept = 0;
};

} // namespace tpmkit
