#pragma once

#include <tpmkit/encoding/hex.h>
#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/provider.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace tpmkit::examples {

constexpr std::uint32_t default_platform_pcr_count = 24U;
constexpr std::string_view default_tcti_config = "tabrmd:bus_type=system";

[[nodiscard]] inline std::vector<std::uint8_t> make_event_bytes(const std::string_view text)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size());
    for (const char ch : text) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }

    return bytes;
}

inline void print_error(const error& error_value)
{
    std::cerr << error_category_name(error_value.category) << ": " << error_value.message << "\n";
}

[[nodiscard]] inline outcome<std::vector<std::uint8_t>>
read_pcr_digest(pcr::provider& provider, const hash_algorithm algorithm, const pcr::index index)
{
    const auto read = provider.read(pcr::selection{algorithm, {index}});
    if (!read.has_value()) {
        return tl::unexpected(read.error());
    }

    if (read->values.empty()) {
        return tl::unexpected(
            error{error_category::resource_error, "requested PCR bank did not return a value"});
    }

    return read->values.front().digest.digest();
}

} // namespace tpmkit::examples
