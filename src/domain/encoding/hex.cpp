#include <tpmkit/encoding/hex.h>

#include <limits>
#include <optional>
#include <stdexcept>

namespace tpmkit::encoding {
namespace {

constexpr char hex_digits[] = "0123456789abcdef";
constexpr std::size_t hex_digits_per_byte = 2U;

[[nodiscard]] std::optional<std::uint8_t> decode_hex_digit(const char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<std::uint8_t>(ch - '0');
    }

    if (ch >= 'a' && ch <= 'f') {
        return static_cast<std::uint8_t>(10U + static_cast<unsigned int>(ch - 'a'));
    }

    if (ch >= 'A' && ch <= 'F') {
        return static_cast<std::uint8_t>(10U + static_cast<unsigned int>(ch - 'A'));
    }

    return std::nullopt;
}

} // namespace

std::string encode_hex(const gsl::span<const std::uint8_t> bytes)
{
    if (bytes.size() > (std::numeric_limits<std::size_t>::max() / hex_digits_per_byte)) {
        throw std::length_error{"hex output size overflow"};
    }

    std::string text;
    const std::size_t output_size = bytes.size() * hex_digits_per_byte;
    if (output_size > text.max_size()) {
        throw std::length_error{"hex output size overflow"};
    }

    text.reserve(output_size);
    for (const std::uint8_t byte : bytes) {
        const unsigned int value = byte;
        const std::size_t high = static_cast<std::size_t>((value >> 4U) & 0x0fU);
        const std::size_t low = static_cast<std::size_t>(value & 0x0fU);

        text.push_back(hex_digits[high]);
        text.push_back(hex_digits[low]);
    }

    return text;
}

outcome<std::vector<std::uint8_t>> decode_hex(const std::string_view text)
{
    if ((text.size() % hex_digits_per_byte) != 0U) {
        return tl::unexpected(
            error{error_category::input_error, "hex input must contain an even number of digits"});
    }

    const std::size_t byte_count = text.size() / hex_digits_per_byte;
    std::vector<std::uint8_t> bytes;
    if (byte_count > bytes.max_size()) {
        return tl::unexpected(error{error_category::input_error, "hex input is too large"});
    }

    bytes.reserve(byte_count);
    for (std::size_t index = 0U; index < text.size(); index += hex_digits_per_byte) {
        const std::optional<std::uint8_t> high = decode_hex_digit(text[index]);
        const std::optional<std::uint8_t> low = decode_hex_digit(text[index + 1U]);
        if (!high.has_value() || !low.has_value()) {
            return tl::unexpected(
                error{error_category::input_error, "hex input contains a non-hex character"});
        }

        const unsigned int value =
            (static_cast<unsigned int>(*high) << 4U) | static_cast<unsigned int>(*low);
        bytes.push_back(static_cast<std::uint8_t>(value));
    }

    return bytes;
}

} // namespace tpmkit::encoding
