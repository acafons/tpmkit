#pragma once

/**
 * @file tpmkit/logging/noop_logger.h
 * @brief No-op logger adapter for callers that disable logging.
 *
 * Declares the default sink used when no concrete logger is supplied.
 */

#include <tpmkit/logging/logger.h>

#include <string_view>

namespace tpmkit {

/**
 * @brief Logger adapter that discards every record.
 *
 * Use this adapter when a component needs a concrete logger object but the
 * caller has intentionally disabled logging.
 *
 * @thread_safety Thread-safe. The adapter has no mutable state.
 * @exception_safety All operations are noexcept.
 * @see logger
 * @since v0.1
 */
class noop_logger final : public logger {
public:
    /**
     * @brief Return the shared stateless no-op logger instance.
     *
     * @return Reference to an immutable no-op logger that lives until process shutdown.
     * @thread_safety Thread-safe. Initialization is guaranteed once by C++.
     * @exception_safety noexcept.
     * @since v0.1
     */
    [[nodiscard]] static noop_logger& instance() noexcept
    {
        static noop_logger log;
        return log;
    }

    /**
     * @brief Discard one structured log record.
     *
     * @param[in] level Ignored severity.
     * @param[in] message Ignored message.
     * @param[in] fields Ignored field list.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept.
     */
    void log(
        log_level level,
        std::string_view message,
        gsl::span<const log_field> fields) noexcept final
    {
        static_cast<void>(level);
        static_cast<void>(message);
        static_cast<void>(fields);
    }
};

} // namespace tpmkit
