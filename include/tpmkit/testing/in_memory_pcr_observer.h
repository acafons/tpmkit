#pragma once

/**
 * @file tpmkit/testing/in_memory_pcr_observer.h
 * @brief In-memory PCR observer adapter for tests.
 *
 * Declares the PCR measurement observer shipped by the `tpmkit_testing` target.
 */

#include <tpmkit/pcr/observer.h>
#include <tpmkit/testing/testing_api.h>

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @namespace tpmkit::testing
 * @brief Public test substitutes and adapters for tpmkit consumers.
 *
 * @warning Experimental - API may change before 1.0.0.
 */
namespace tpmkit::testing {

/**
 * @brief PCR measurement operation recorded by in_memory_pcr_observer.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible.
 * @exception_safety No operations throw.
 * @since v0.1
 */
enum class pcr_measurement_operation {
    /** @brief Measurement came from PCR_Extend. */
    extend,
    /** @brief Measurement came from PCR_Event. */
    event,
};

/**
 * @brief Captured PCR measurement record.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible. Independent records may be used
 * concurrently; shared records require external synchronization.
 * @exception_safety Follows the guarantees of `std::vector` member operations.
 * @since v0.1
 */
struct pcr_measurement_record {
    /** @brief Target PCR register. */
    pcr_index index;
    /** @brief Operation that produced this record. */
    pcr_measurement_operation operation;
    /** @brief Extend input digests, or event result digests. */
    std::vector<pcr_digest_value> digests;
    /** @brief Raw event bytes for PCR_Event; empty for PCR_Extend. */
    std::vector<std::uint8_t> event_data;
};

/**
 * @brief In-memory PCR observer adapter for test assertions.
 *
 * in_memory_pcr_observer copies every successful extend or event notification
 * into an internal vector so tests can inspect recorded measurements.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible. Shared instances require external
 * synchronization.
 * @exception_safety Observer callbacks are noexcept and drop records on
 * allocation failure; query methods follow `std::vector` guarantees.
 * @see pcr_observer
 * @since v0.1
 */
class TPMKIT_TESTING_API in_memory_pcr_observer final : public pcr_observer {
public:
    /**
     * @brief Remove every captured measurement.
     *
     * @thread_safety Thread-compatible.
     * @exception_safety Basic; the observer remains valid if vector operations
     * throw.
     */
    void clear();

    /**
     * @brief Return the number of captured measurements.
     *
     * @return Number of records currently stored.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t count() const noexcept;

    /**
     * @brief Return every captured measurement in capture order.
     *
     * @return Const reference valid until the next non-const operation.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] const std::vector<pcr_measurement_record>& entries() const noexcept;

    /**
     * @brief Return captured measurements for one PCR index.
     *
     * @param[in] index PCR index to match.
     * @return Owning copy of matching records in capture order.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; copy failure leaves the observer unchanged.
     */
    [[nodiscard]] std::vector<pcr_measurement_record> entries_by_index(pcr_index index) const;

    /**
     * @brief Capture one successful PCR event notification.
     *
     * @param[in] index PCR index that was extended.
     * @param[in] event_data Raw event bytes supplied to the TPM.
     * @param[in] result Digest values returned by the event operation.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept; allocation failures drop the record.
     */
    void on_event(pcr_index index, gsl::span<const std::uint8_t> event_data,
                  const pcr_event_result& result) noexcept final;

    /**
     * @brief Capture one successful PCR extend notification.
     *
     * @param[in] index PCR index that was extended.
     * @param[in] digests Digest values supplied to the extend operation.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept; allocation failures drop the record.
     */
    void on_extend(pcr_index index, gsl::span<const pcr_digest_value> digests) noexcept final;

private:
    std::vector<pcr_measurement_record> records_;
};

} // namespace tpmkit::testing
