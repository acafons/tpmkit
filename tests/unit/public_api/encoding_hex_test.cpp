#include <tpmkit/encoding/hex.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

TEST(encoding_hex, encodes_empty_bytes_as_empty_text)
{
    // Verifies empty byte input produces empty hexadecimal text.

    const std::vector<std::uint8_t> bytes;

    EXPECT_TRUE(tpmkit::encoding::encode_hex(gsl::span<const std::uint8_t>{bytes}).empty());
}

TEST(encoding_hex, encodes_bytes_as_lowercase_two_digit_hex)
{
    // Verifies each byte is encoded with two lowercase hexadecimal digits.

    const std::array<std::uint8_t, 4U> bytes{{0x00U, 0x0fU, 0xa5U, 0xffU}};

    EXPECT_EQ(tpmkit::encoding::encode_hex(gsl::span<const std::uint8_t>{bytes}),
              std::string{"000fa5ff"});
}

TEST(encoding_hex, decodes_empty_text_as_empty_bytes)
{
    // Verifies empty hexadecimal text produces an empty byte vector.

    const tpmkit::outcome<std::vector<std::uint8_t>> decoded =
        tpmkit::encoding::decode_hex(std::string_view{});

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->empty());
}

TEST(encoding_hex, decodes_uppercase_and_lowercase_digits)
{
    // Verifies decoding accepts both uppercase and lowercase hexadecimal digits.

    const tpmkit::outcome<std::vector<std::uint8_t>> decoded =
        tpmkit::encoding::decode_hex("000FA5ff");
    const std::vector<std::uint8_t> expected{0x00U, 0x0fU, 0xa5U, 0xffU};

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, expected);
}

TEST(encoding_hex, rejects_odd_digit_count)
{
    // Verifies hexadecimal input must contain a whole number of bytes.

    const tpmkit::outcome<std::vector<std::uint8_t>> decoded = tpmkit::encoding::decode_hex("abc");

    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error().category, tpmkit::error_category::input_error);
}

TEST(encoding_hex, rejects_non_hexadecimal_characters)
{
    // Verifies malformed hexadecimal text is reported as caller input error.

    const tpmkit::outcome<std::vector<std::uint8_t>> decoded = tpmkit::encoding::decode_hex("00xz");

    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error().category, tpmkit::error_category::input_error);
}

TEST(encoding_hex, round_trips_encoded_bytes)
{
    // Verifies bytes encoded by the helper can be decoded back to the same value.

    const std::vector<std::uint8_t> bytes{0x01U, 0x23U, 0x45U, 0x67U, 0x89U, 0xabU, 0xcdU, 0xefU};

    const std::string encoded = tpmkit::encoding::encode_hex(gsl::span<const std::uint8_t>{bytes});
    const tpmkit::outcome<std::vector<std::uint8_t>> decoded =
        tpmkit::encoding::decode_hex(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, bytes);
}

} // namespace
