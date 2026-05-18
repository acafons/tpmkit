#include <tpmkit/spdlog_logger.h>

#include <spdlog/spdlog.h>

#include <cctype>
#include <string>
#include <string_view>

namespace {

// Escapes a field value for key=value rendering.
// Values containing whitespace, '=', '"', '\' or control characters are
// wrapped in double quotes with internal escaping applied.
std::string escape_value(const std::string_view value)
{
    bool needs_quoting = false;
    for (const char c : value) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || c == ' ' || c == '=' || c == '"' || c == '\\') {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting) {
        return std::string{value};
    }

    std::string out;
    out.reserve(value.size() + 2U);
    out += '"';
    for (const char c : value) {
        const auto u = static_cast<unsigned char>(c);
        if (c == '"')      { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else if (c == '\t') { out += "\\t"; }
        else if (u < 0x20) {
            constexpr std::string_view hex = "0123456789abcdef";
            out += "\\x";
            out += hex[(u >> 4U) & 0xfU];
            out += hex[u & 0xfU];
        } else {
            out += c;
        }
    }
    out += '"';
    return out;
}

::spdlog::level::level_enum to_spdlog_level(const tpmkit::log_level level) noexcept
{
    switch (level) {
        case tpmkit::log_level::trace: return ::spdlog::level::trace;
        case tpmkit::log_level::debug: return ::spdlog::level::debug;
        case tpmkit::log_level::info:  return ::spdlog::level::info;
        case tpmkit::log_level::warn:  return ::spdlog::level::warn;
        case tpmkit::log_level::error: return ::spdlog::level::err;
    }
    return ::spdlog::level::info;
}

} // namespace

namespace tpmkit {

spdlog_logger::spdlog_logger(std::shared_ptr<::spdlog::logger> sink)
    : sink_{std::move(sink)}
{
}

spdlog_logger::~spdlog_logger() = default;

spdlog_logger::spdlog_logger(spdlog_logger&&) noexcept = default;
spdlog_logger& spdlog_logger::operator=(spdlog_logger&&) noexcept = default;

void spdlog_logger::log(
    const log_level level,
    const std::string_view message,
    const gsl::span<const log_field> fields) noexcept
{
    try {
        const auto sl = to_spdlog_level(level);
        if (!sink_->should_log(sl)) {
            return;
        }

        std::string rendered{message};
        rendered.reserve(message.size() + fields.size() * 32U);
        for (const auto& f : fields) {
            rendered += ' ';
            rendered += f.key;
            rendered += '=';
            rendered += escape_value(f.value);
        }

        sink_->log(sl, rendered);
    } catch (...) {
        // Logger port is noexcept; swallow all failures.
    }
}

void spdlog_logger::flush() noexcept
{
    try {
        sink_->flush();
    } catch (...) {
        // Logger port is noexcept; flush failures are swallowed.
    }
}

} // namespace tpmkit
