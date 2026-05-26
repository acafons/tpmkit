#pragma once

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/provider.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpmkit::examples {

constexpr std::uint32_t default_platform_pcr_count = 24U;
constexpr std::string_view default_tcti_config = "tabrmd:bus_type=system";

[[nodiscard]] inline std::string_view algorithm_name(const hash_algorithm algorithm) noexcept
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        return "sha1";
    case hash_algorithm::sha256:
        return "sha256";
    case hash_algorithm::sha384:
        return "sha384";
    case hash_algorithm::sha512:
        return "sha512";
    }

    return "unknown";
}

[[nodiscard]] inline std::vector<std::uint8_t> bytes_from_text(const std::string_view text)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size());
    for (const char ch : text) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }

    return bytes;
}

[[nodiscard]] inline std::string_view category_name(const error_category category) noexcept
{
    switch (category) {
    case error_category::input_error:
        return "input_error";
    case error_category::security_failure:
        return "security_failure";
    case error_category::resource_error:
        return "resource_error";
    case error_category::backend_error:
        return "backend_error";
    }

    return "unknown";
}

[[nodiscard]] inline outcome<tpm_context> create_context(
    std::string tcti_config,
    const tpm_context_config::startup_mode startup = tpm_context_config::startup_mode::clear)
{
    tpm_context_config config;
    config.tcti = tcti_string_config{std::move(tcti_config)};
    config.startup = startup;
    return tpm_context::create(std::move(config));
}

[[nodiscard]] inline std::string hex_encode(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const std::uint8_t byte : bytes) {
        out << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return out.str();
}

[[nodiscard]] inline std::optional<std::uint8_t> hex_nibble(const char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<std::uint8_t>(ch - '0');
    }

    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (lower >= 'a' && lower <= 'f') {
        return static_cast<std::uint8_t>(10U + static_cast<unsigned int>(lower - 'a'));
    }

    return std::nullopt;
}

[[nodiscard]] inline outcome<std::vector<std::uint8_t>> hex_decode(const std::string_view hex)
{
    if ((hex.size() % 2U) != 0U) {
        return tl::unexpected(
            error{error_category::input_error, "hex input must contain an even number of digits"});
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2U);
    for (std::size_t index = 0U; index < hex.size(); index += 2U) {
        const auto high = hex_nibble(hex[index]);
        const auto low = hex_nibble(hex[index + 1U]);
        if (!high.has_value() || !low.has_value()) {
            return tl::unexpected(
                error{error_category::input_error, "hex input contains a non-hex character"});
        }

        bytes.push_back(static_cast<std::uint8_t>((*high << 4U) | *low));
    }

    return bytes;
}

[[nodiscard]] inline std::set<pcr_index> make_pcr_range(const std::uint32_t first,
                                                        const std::uint32_t count)
{
    std::set<pcr_index> indices;
    for (std::uint32_t offset = 0U; offset < count; ++offset) {
        indices.insert(pcr_index{first + offset});
    }

    return indices;
}

inline void print_error(const error& error_value)
{
    std::cerr << category_name(error_value.category) << ": " << error_value.message << "\n";
}

[[nodiscard]] inline outcome<std::vector<std::uint8_t>>
read_pcr_digest(pcr_provider& provider, const hash_algorithm algorithm, const pcr_index index)
{
    const auto read = provider.read(pcr_selection{algorithm, {index}});
    if (!read.has_value()) {
        return tl::unexpected(read.error());
    }

    if (read.value().values.empty()) {
        return tl::unexpected(
            error{error_category::resource_error, "requested PCR bank did not return a value"});
    }

    return read.value().values.front().digest.digest();
}

} // namespace tpmkit::examples
