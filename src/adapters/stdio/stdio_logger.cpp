#include <tpmkit/logging/stdio_logger.h>

#include "adapters/common/escape_value.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr std::size_t initial_record_capacity = 256U;
constexpr std::string_view reset_sequence = "\x1b[0m";

bool utc_time_from_epoch_seconds(const std::time_t seconds, std::tm& utc) noexcept
{
#if defined(_WIN32)
    return ::gmtime_s(&utc, &seconds) == 0;
#else
    return ::gmtime_r(&seconds, &utc) != nullptr;
#endif
}

const char* ansi_sequence(const tpmkit::log_level level) noexcept
{
    switch (level) {
    case tpmkit::log_level::trace:
        return "\x1b[2m";
    case tpmkit::log_level::debug:
        return "\x1b[36m";
    case tpmkit::log_level::info:
        return "\x1b[32m";
    case tpmkit::log_level::warn:
        return "\x1b[33m";
    case tpmkit::log_level::error:
        return "\x1b[31m";
    }
    return "";
}

void append_timestamp(std::string& record)
{
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto seconds_point = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds_point);
    const std::time_t seconds = clock::to_time_t(seconds_point);

    std::tm utc{};
    if (!utc_time_from_epoch_seconds(seconds, utc)) {
        record += "1970-01-01T00:00:00.000Z";
        return;
    }

    char buffer[25]{};
    const int written = std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                                      utc.tm_min, utc.tm_sec, static_cast<int>(millis.count()));
    if (written <= 0) {
        record += "1970-01-01T00:00:00.000Z";
        return;
    }

    record.append(buffer, static_cast<std::size_t>(written));
}

bool env_is_set(const char* const name) noexcept
{
    const char* const value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

bool is_below_min_level(const tpmkit::log_level level,
                        const std::optional<tpmkit::log_level> min_level) noexcept
{
    if (!min_level.has_value()) {
        return false;
    }

    return static_cast<int>(level) < static_cast<int>(*min_level);
}

bool is_tty_stream(const std::ostream* const stream) noexcept
{
    if (stream == &std::cout) {
#if defined(_WIN32)
        return ::_isatty(::_fileno(stdout)) != 0;
#else
        return ::isatty(::fileno(stdout)) == 1;
#endif
    }
    if (stream == &std::cerr) {
#if defined(_WIN32)
        return ::_isatty(::_fileno(stderr)) != 0;
#else
        return ::isatty(::fileno(stderr)) == 1;
#endif
    }
    return false;
}

std::string_view level_label(const tpmkit::log_level level) noexcept
{
    switch (level) {
    case tpmkit::log_level::trace:
        return "TRACE";
    case tpmkit::log_level::debug:
        return "DEBUG";
    case tpmkit::log_level::info:
        return "INFO ";
    case tpmkit::log_level::warn:
        return "WARN ";
    case tpmkit::log_level::error:
        return "ERROR";
    }
    return "INFO ";
}

bool should_colorize(const tpmkit::color_mode mode, const std::ostream* const stream) noexcept
{
    switch (mode) {
    case tpmkit::color_mode::always:
        return true;
    case tpmkit::color_mode::never:
        return false;
    case tpmkit::color_mode::auto_:
        break;
    }

    if (env_is_set("NO_COLOR")) {
        return false;
    }
    if (env_is_set("FORCE_COLOR")) {
        return true;
    }
    return is_tty_stream(stream);
}

std::ostream* stream_for_level(const tpmkit::log_level level, std::ostream* const out,
                               std::ostream* const err) noexcept
{
    switch (level) {
    case tpmkit::log_level::warn:
    case tpmkit::log_level::error:
        return err;
    case tpmkit::log_level::trace:
    case tpmkit::log_level::debug:
    case tpmkit::log_level::info:
        return out;
    }
    return out;
}

} // namespace

namespace tpmkit {

stdio_logger::stdio_logger() noexcept : stdio_logger(stdio_logger_options{}) {}

stdio_logger::stdio_logger(const stdio_logger_options opts)
    : color_{opts.color}, err_{opts.err == nullptr ? &std::cerr : opts.err},
      min_level_{opts.min_level}, out_{opts.out == nullptr ? &std::cout : opts.out}
{}

stdio_logger::~stdio_logger() noexcept = default;

stdio_logger::stdio_logger(stdio_logger&& other) noexcept
{
    const std::lock_guard<std::mutex> lock{other.mu_};
    color_ = other.color_;
    err_ = other.err_;
    min_level_ = other.min_level_;
    out_ = other.out_;
    other.err_ = nullptr;
    other.out_ = nullptr;
}

stdio_logger& stdio_logger::operator=(stdio_logger&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    std::unique_lock<std::mutex> this_lock{mu_, std::defer_lock};
    std::unique_lock<std::mutex> other_lock{other.mu_, std::defer_lock};
    std::lock(this_lock, other_lock);

    color_ = other.color_;
    err_ = other.err_;
    min_level_ = other.min_level_;
    out_ = other.out_;
    record_.clear();
    other.err_ = nullptr;
    other.out_ = nullptr;
    return *this;
}

void stdio_logger::log(const log_level level, const std::string_view message,
                       const gsl::span<const log_field> fields) noexcept
{
    try {
        const std::lock_guard<std::mutex> lock{mu_};
        if (is_below_min_level(level, min_level_)) {
            return;
        }

        std::ostream* const stream = stream_for_level(level, out_, err_);
        if (stream == nullptr) {
            return;
        }

        if (record_.capacity() < initial_record_capacity) {
            record_.reserve(initial_record_capacity);
        }
        record_.clear();

        append_timestamp(record_);
        record_ += ' ';
        const bool color = should_colorize(color_, stream);
        if (color) {
            record_ += ansi_sequence(level);
        }
        record_ += '[';
        record_ += level_label(level);
        record_ += ']';
        if (color) {
            record_ += reset_sequence;
        }
        record_ += ' ';
        record_ += message;

        for (const auto& field : fields) {
            record_ += ' ';
            record_ += field.key;
            record_ += '=';
            record_ += detail::log_format::escape_value(field.value);
        }

        *stream << record_ << '\n' << std::flush;
    } catch (...) {
        // Logger port is noexcept; swallow all formatting and stream failures.
    }
}

} // namespace tpmkit
