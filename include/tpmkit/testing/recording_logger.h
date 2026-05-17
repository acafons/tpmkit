#pragma once

/**
 * @file tpmkit/testing/recording_logger.h
 * @brief In-memory structured logger adapter for tests.
 *
 * Declares the recording logger shipped by the `tpmkit_testing` target.
 */

#include <tpmkit/logger.h>
#include <tpmkit/testing/testing_api.h>

#include <mutex>
#include <string>
#include <utility>
#include <vector>

/**
 * @namespace tpmkit::testing
 * @brief Public test substitutes and adapters for tpmkit consumers.
 *
 * @warning Experimental - API may change before 1.0.0.
 */
namespace tpmkit::testing {

/**
 * @brief Captured shape returned by recording_logger::snapshot().
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-compatible. Independent records may be used
 * concurrently; shared records require external synchronization.
 * @exception_safety Follows the guarantees of `std::string` and `std::vector`
 * member operations.
 * @since v0.1
 */
struct log_record {
    /** @brief Captured severity. */
    log_level level;
    /** @brief Captured message copied from the log call. */
    std::string message;
    /** @brief Captured fields copied as owning key/value pairs. */
    std::vector<std::pair<std::string, std::string>> fields;
};

/**
 * @brief Thread-safe in-memory logger adapter for tests.
 *
 * recording_logger copies each record into an internal vector so tests can
 * assert on levels and fields without retaining views into caller storage.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-safe. `log`, `snapshot`, and `clear` serialize access
 * to captured records.
 * @exception_safety `log` is noexcept and drops records on allocation failure;
 * `snapshot` and `clear` provide the basic guarantee.
 * @see logger
 * @since v0.1
 */
class TPMKIT_TESTING_API recording_logger final : public logger {
public:
    /**
     * @brief Remove every captured record.
     *
     * @thread_safety Thread-safe.
     * @exception_safety Basic; the logger remains valid if vector operations
     * throw.
     */
    void clear();

    /**
     * @brief Capture one structured record.
     *
     * @param[in] level Severity selected by the call site.
     * @param[in] message Message or event name to copy.
     * @param[in] fields Non-owning field list to copy before returning.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept; allocation failures drop the record.
     */
    void log(log_level level, std::string_view message,
             gsl::span<const log_field> fields) noexcept final;

    /**
     * @brief Return an isolated copy of the records captured so far.
     *
     * @return Owning snapshot copy. Later log or clear calls do not modify the
     * returned vector.
     * @thread_safety Thread-safe.
     * @exception_safety Strong; copy failure leaves the logger unchanged.
     */
    [[nodiscard]] std::vector<log_record> snapshot() const;

private:
    mutable std::mutex mu_;
    std::vector<log_record> records_;
};

} // namespace tpmkit::testing
