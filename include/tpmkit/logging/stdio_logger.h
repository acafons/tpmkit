#pragma once

/**
 * @file tpmkit/logging/stdio_logger.h
 * @brief Zero-dependency stdout/stderr adapter for the tpmkit logger port.
 *
 * The adapter renders structured records as single-line text with an ISO 8601
 * UTC timestamp, a fixed-width bracketed level token, message text, and
 * `key=value` fields. Records with `error` or `warn` severity are routed to the
 * error stream; lower severities are routed to the output stream.
 */

#include <tpmkit/logging/logger.h>
#include <tpmkit/logging/stdio_api.h>

#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace tpmkit {

/**
 * @brief ANSI color handling mode for stdio log records.
 *
 * Color is applied only to the bracketed level token. In automatic mode,
 * `NO_COLOR` disables color and `FORCE_COLOR` enables it when `NO_COLOR` is not
 * set.
 *
 * @thread_safety Enum values are immutable and thread-safe.
 * @exception_safety No operations throw.
 * @since v0.1
 */
enum class color_mode {
    /** @brief Enable color only when the selected stream is a TTY, subject to environment
       overrides. */
    auto_,
    /** @brief Always emit ANSI color for the level token and ignore color environment variables. */
    always,
    /** @brief Never emit ANSI color and ignore color environment variables. */
    never,
};

/**
 * @brief Construction options for stdio_logger.
 *
 * Non-null stream pointers are borrowed and must outlive the logger. A null
 * output stream selects `std::cout`; a null error stream selects `std::cerr`.
 *
 * @thread_safety Thread-compatible. Configure options before constructing the
 * logger and then treat them as immutable.
 * @exception_safety No member operation throws.
 * @since v0.1
 */
struct stdio_logger_options {
    /** @brief Optional runtime threshold; records below the threshold are dropped. */
    std::optional<log_level> min_level;
    /** @brief ANSI color handling mode. */
    color_mode color = color_mode::auto_;
    /** @brief Borrowed output stream for trace, debug, and info records; null selects `std::cout`.
     */
    std::ostream* out = nullptr;
    /** @brief Borrowed error stream for warn and error records; null selects `std::cerr`. */
    std::ostream* err = nullptr;
};

/**
 * @brief Logger adapter that writes tpmkit records to stdout and stderr streams.
 *
 * The adapter is move-only, thread-safe for concurrent `log()` calls, and
 * swallows every formatting or stream failure at the `noexcept` boundary. Moved
 * instances are valid no-op loggers.
 *
 * @thread_safety Thread-safe for `log()` calls on a shared instance. Move
 * operations are single-threaded for the moved instances.
 * @exception_safety `log`, destruction, default construction, and moves are
 * noexcept. Sink failures are swallowed.
 * @see logger
 * @since v0.1
 */
class TPMKIT_STDIO_API stdio_logger final : public logger {
public:
    /**
     * @brief Construct a logger that writes to `std::cout` and `std::cerr`.
     *
     * @thread_safety Thread-safe after construction.
     * @exception_safety noexcept.
     */
    stdio_logger() noexcept;

    /**
     * @brief Construct a logger with explicit stream, filter, and color options.
     *
     * @param[in] opts Options for routing, filtering, and color. Non-null stream
     *                 pointers are borrowed and must outlive the logger.
     * @thread_safety Thread-safe after construction.
     * @exception_safety Strong; construction does not take ownership of streams.
     */
    explicit stdio_logger(stdio_logger_options opts);

    /**
     * @brief Destroy the adapter.
     *
     * @exception_safety noexcept.
     */
    ~stdio_logger() noexcept override;

    /**
     * @brief Copy construction is disabled because the adapter owns synchronization state.
     *
     * @param[in] other Source adapter; copying is intentionally unavailable.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    stdio_logger(const stdio_logger& other) = delete;

    /**
     * @brief Copy assignment is disabled because the adapter owns synchronization state.
     *
     * @param[in] other Source adapter; copying is intentionally unavailable.
     * @return Reference to this adapter if the operation existed.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    stdio_logger& operator=(const stdio_logger& other) = delete;

    /**
     * @brief Move-construct a stdio adapter.
     *
     * @param[in] other Source adapter. Valid no-op logger after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    stdio_logger(stdio_logger&& other) noexcept;

    /**
     * @brief Move-assign a stdio adapter.
     *
     * @param[in] other Source adapter. Valid no-op logger after the move.
     * @return Reference to this adapter.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    stdio_logger& operator=(stdio_logger&& other) noexcept;

    /**
     * @brief Write one structured record to the routed stream.
     *
     * Records below `stdio_logger_options::min_level` are dropped before
     * formatting. Field values are rendered as `key=value` pairs with the shared
     * adapter escaping rules.
     *
     * @param[in] level Severity of the record.
     * @param[in] message Stable event name or diagnostic text.
     * @param[in] fields Non-owning field list valid for this call only.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept; formatting or stream failures are swallowed.
     */
    void log(log_level level, std::string_view message,
             gsl::span<const log_field> fields) noexcept override;

private:
    color_mode color_;
    std::ostream* err_;
    std::optional<log_level> min_level_;
    mutable std::mutex mu_;
    std::ostream* out_;
    std::string record_;
};

} // namespace tpmkit
