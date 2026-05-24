#pragma once

/**
 * @file tpmkit/logging/spdlog_logger.h
 * @brief spdlog adapter for the tpmkit logger port.
 *
 * Consumers that use spdlog as their logging framework wire this adapter at the
 * composition root and pass a reference to it to every tpmkit adapter that
 * accepts a `logger&`. The adapter is move-only and thread-safe.
 *
 * The `::spdlog::logger` type is forward-declared here so this header does not
 * pull spdlog headers into the public include surface. Callers that construct
 * a `spdlog_logger` must include `<spdlog/spdlog.h>` themselves.
 *
 * @note Production spdlog includes are limited to
 * `src/adapters/logging/spdlog/`. This header is intentionally free of spdlog
 * includes.
 */

#include <tpmkit/logging/logger.h>
#include <tpmkit/logging/spdlog_api.h>

#include <memory>
#include <mutex>

namespace spdlog {
class logger;
} // namespace spdlog

namespace tpmkit {

/**
 * @brief Logger adapter that forwards tpmkit records to an spdlog logger.
 *
 * Each `log()` call renders the structured fields as `key=value` pairs
 * appended to the message, then forwards the formatted line to the wrapped
 * `::spdlog::logger`. Values containing whitespace, `=`, or control characters
 * are quoted and escaped by the adapter before rendering.
 *
 * The adapter does not emit timestamps, hostnames, PIDs, or correlation IDs;
 * those are added by spdlog's pattern formatter at the sink level.
 *
 * @thread_safety Thread-safe. Concurrent `log()` and `flush()` calls made
 * through this adapter are serialized before reaching the wrapped spdlog
 * logger. Direct concurrent use of the same spdlog logger outside this adapter
 * remains the caller's responsibility.
 * @exception_safety All operations are noexcept; sink failures are swallowed.
 * @see logger
 * @since v0.1
 */
class TPMKIT_SPDLOG_API spdlog_logger final : public logger {
public:
    /**
     * @brief Construct a logger wrapping an existing spdlog logger.
     *
     * @param[in] sink Shared spdlog logger. Must not be null.
     * @throws tpmkit_error if `sink` is null.
     * @exception_safety Strong; failed construction has no side effects.
     */
    explicit spdlog_logger(std::shared_ptr<::spdlog::logger> sink);

    /**
     * @brief Destroy the adapter.
     *
     * Defined in the implementation file so the `::spdlog::logger` destructor
     * runs only where the type is complete.
     *
     * @exception_safety noexcept.
     */
    ~spdlog_logger() override;

    /**
     * @brief Move-construct a spdlog adapter.
     *
     * @param[in] other Source adapter. Valid but unspecified after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    spdlog_logger(spdlog_logger&& other) noexcept;

    /**
     * @brief Move-assign a spdlog adapter.
     *
     * @param[in] other Source adapter. Valid but unspecified after the move.
     * @return Reference to this adapter.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    spdlog_logger& operator=(spdlog_logger&& other) noexcept;

    /**
     * @brief Copy construction is disabled because the adapter owns synchronization state.
     *
     * @param[in] other Source adapter; copying is intentionally unavailable.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    spdlog_logger(const spdlog_logger& other) = delete;

    /**
     * @brief Copy assignment is disabled because the adapter owns synchronization state.
     *
     * @param[in] other Source adapter; copying is intentionally unavailable.
     * @return Reference to this adapter if the operation existed.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    spdlog_logger& operator=(const spdlog_logger& other) = delete;

    /**
     * @brief Forward one structured record to the wrapped spdlog logger.
     *
     * The record is filtered against the spdlog logger's active level before
     * rendering. Fields are rendered as `key=value` pairs appended after the
     * message, separated by spaces.
     *
     * @param[in] level Severity of the record.
     * @param[in] message Stable event name or diagnostic text.
     * @param[in] fields Non-owning field list valid for this call only.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept; rendering or sink failures are swallowed.
     */
    void log(log_level level, std::string_view message,
             gsl::span<const log_field> fields) noexcept override;

    /**
     * @brief Flush pending records in the underlying spdlog logger.
     *
     * Useful at composition-root teardown to drain any async sink queue before
     * destruction.
     *
     * @thread_safety Thread-safe.
     * @exception_safety noexcept; flush failures are swallowed.
     */
    void flush() noexcept;

private:
    mutable std::mutex mu_;
    std::shared_ptr<::spdlog::logger> sink_;
};

} // namespace tpmkit
