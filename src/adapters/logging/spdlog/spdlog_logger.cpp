#include <tpmkit/logging/spdlog_logger.h>

#include "adapters/common/escape_value.h"

#include <tpmkit/exception.h>

#include <spdlog/spdlog.h>

#include <mutex>
#include <string>
#include <string_view>

namespace {

::spdlog::level::level_enum to_spdlog_level(const tpmkit::log_level level) noexcept
{
    switch (level) {
    case tpmkit::log_level::trace:
        return ::spdlog::level::trace;
    case tpmkit::log_level::debug:
        return ::spdlog::level::debug;
    case tpmkit::log_level::info:
        return ::spdlog::level::info;
    case tpmkit::log_level::warn:
        return ::spdlog::level::warn;
    case tpmkit::log_level::error:
        return ::spdlog::level::err;
    }
    return ::spdlog::level::info;
}

} // namespace

namespace tpmkit {

spdlog_logger::spdlog_logger(std::shared_ptr<::spdlog::logger> sink) : sink_{std::move(sink)}
{
    if (sink_ == nullptr) {
        throw tpmkit_error{"spdlog_logger sink must not be null"};
    }
}

spdlog_logger::~spdlog_logger() = default;

spdlog_logger::spdlog_logger(spdlog_logger&& other) noexcept
{
    const std::lock_guard<std::mutex> lock{other.mu_};
    sink_ = std::move(other.sink_);
}

spdlog_logger& spdlog_logger::operator=(spdlog_logger&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    const std::scoped_lock lock{mu_, other.mu_};
    sink_ = std::move(other.sink_);
    return *this;
}

void spdlog_logger::log(const log_level level, const std::string_view message,
                        const gsl::span<const log_field> fields) noexcept
{
    try {
        const auto sl = to_spdlog_level(level);
        const std::lock_guard<std::mutex> lock{mu_};
        if (sink_ == nullptr) {
            return;
        }

        if (!sink_->should_log(sl)) {
            return;
        }

        std::string rendered{message};
        rendered.reserve(message.size() + fields.size() * 32U);
        for (const auto& f : fields) {
            rendered += ' ';
            rendered += f.key;
            rendered += '=';
            rendered += detail::log_format::escape_value(f.value);
        }

        sink_->log(sl, rendered);
    } catch (...) {
        // Logger port is noexcept; swallow all failures.
    }
}

void spdlog_logger::flush() noexcept
{
    try {
        const std::lock_guard<std::mutex> lock{mu_};
        if (sink_ == nullptr) {
            return;
        }

        sink_->flush();
    } catch (...) {
        // Logger port is noexcept; flush failures are swallowed.
    }
}

} // namespace tpmkit
