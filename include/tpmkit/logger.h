#pragma once

/**
 * @file tpmkit/logger.h
 * @brief Structured logging port used by tpmkit components.
 *
 * Consumers provide adapters for their logging framework. tpmkit emits
 * structured records through this port and never depends on a concrete logging
 * backend.
 */

#include <gsl/span>

#include <string_view>

namespace tpmkit {

/**
 * @brief Severity assigned to a structured log record.
 *
 * @thread_safety Enum values are immutable and thread-safe.
 * @exception_safety No operations throw.
 * @since v0.1
 */
enum class log_level {
    /** @brief Call-flow detail for integration debugging. */
    trace,
    /** @brief Slow-path diagnostics and adapter decisions. */
    debug,
    /** @brief Lifecycle and significant state-change records. */
    info,
    /** @brief Degraded but continued behavior. */
    warn,
    /** @brief Failed operation paired with a returned or thrown failure. */
    error,
};

/**
 * @brief Single structured key/value field passed to logger adapters.
 *
 * Values are non-owning views that are valid only for the duration of the
 * `logger::log` call. Adapters must copy fields they retain.
 *
 * @thread_safety Thread-compatible. Independent field values may be used
 * concurrently; shared views depend on the lifetime of the referenced storage.
 * @exception_safety No member operation throws.
 * @since v0.1
 */
struct log_field {
    /** @brief Stable snake_case field key. */
    std::string_view key;
    /** @brief Already-stringified field value. */
    std::string_view value;
};

/**
 * @brief Abstract structured logging port.
 *
 * Implementations map tpmkit log records to a concrete sink. The port is
 * `noexcept`: adapters swallow sink failures instead of propagating exceptions
 * into library call sites.
 *
 * @thread_safety Thread-safe. Adapters must serialize internally when their
 * wrapped sink is not thread-safe.
 * @exception_safety Destructor is noexcept; `log` is noexcept.
 * @since v0.1
 */
class logger {
public:
    /**
     * @brief Destroy the logger adapter.
     *
     * @thread_safety Single-threaded for the destroyed adapter unless the
     * adapter documents stronger behavior.
     * @exception_safety noexcept.
     */
    virtual ~logger() = default;

    /**
     * @brief Emit one structured log record.
     *
     * @param[in] level Severity selected by tpmkit for this event.
     * @param[in] message Stable event name or short diagnostic text. Must not
     *                    contain secret-derived data.
     * @param[in] fields Non-owning field list valid only for this call.
     * @thread_safety Thread-safe. Implementations must handle concurrent calls.
     * @exception_safety noexcept; sink failures are swallowed by the adapter.
     */
    virtual void log(
        log_level level,
        std::string_view message,
        gsl::span<const log_field> fields) noexcept = 0;
};

} // namespace tpmkit
