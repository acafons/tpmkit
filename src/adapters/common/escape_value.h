#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tpmkit::detail::log_format {

// Escapes a field value for key=value rendering.
// Values containing whitespace, '=', '"', '\' or control characters are
// wrapped in double quotes with internal escaping applied.
inline std::string escape_value(const std::string_view value)
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
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (u < 0x20) {
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

} // namespace tpmkit::detail::log_format
