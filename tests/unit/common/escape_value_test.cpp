#include "adapters/common/escape_value.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace {

namespace log_format = tpmkit::detail::log_format;

TEST(escape_value, leaves_plain_ascii_unquoted)
{
    // Verifies plain ASCII values without special characters use the fast path.

    EXPECT_EQ(log_format::escape_value("hello"), "hello");
}

TEST(escape_value, leaves_empty_string_unquoted)
{
    // Verifies an empty field value remains an empty string.

    EXPECT_EQ(log_format::escape_value(""), "");
}

TEST(escape_value, quotes_values_containing_space)
{
    // Verifies values containing spaces are wrapped in double quotes.

    EXPECT_EQ(log_format::escape_value("hello world"), "\"hello world\"");
}

TEST(escape_value, quotes_values_containing_equals)
{
    // Verifies values containing '=' are wrapped in double quotes.

    EXPECT_EQ(log_format::escape_value("a=b"), "\"a=b\"");
}

TEST(escape_value, escapes_backslash)
{
    // Verifies embedded backslashes are escaped inside quoted values.

    EXPECT_EQ(log_format::escape_value("a\\b"), "\"a\\\\b\"");
}

TEST(escape_value, escapes_double_quote)
{
    // Verifies embedded double quotes are escaped inside quoted values.

    EXPECT_EQ(log_format::escape_value("a\"b"), "\"a\\\"b\"");
}

TEST(escape_value, escapes_newline)
{
    // Verifies embedded newlines are rendered as the two-character escape.

    EXPECT_EQ(log_format::escape_value("a\nb"), "\"a\\nb\"");
}

TEST(escape_value, escapes_carriage_return)
{
    // Verifies embedded carriage returns are rendered as the two-character escape.

    EXPECT_EQ(log_format::escape_value("a\rb"), "\"a\\rb\"");
}

TEST(escape_value, escapes_tab)
{
    // Verifies embedded tabs are rendered as the two-character escape.

    EXPECT_EQ(log_format::escape_value("a\tb"), "\"a\\tb\"");
}

TEST(escape_value, escapes_embedded_null)
{
    // Verifies embedded null bytes are rendered as hexadecimal control escapes.

    const std::string value{"a\0b", 3U};

    EXPECT_EQ(log_format::escape_value(value), "\"a\\x00b\"");
}

TEST(escape_value, escapes_generic_control_character)
{
    // Verifies non-special control bytes are rendered as hexadecimal escapes.

    const std::string value{"a\x01"
                            "b",
                            3U};

    EXPECT_EQ(log_format::escape_value(value), "\"a\\x01b\"");
}

TEST(escape_value, preserves_high_ascii_byte_inside_quoted_value)
{
    // Verifies high-ASCII bytes are copied unchanged while surrounding quoting applies.

    std::string value{"a "};
    value.push_back(static_cast<char>(static_cast<std::uint8_t>(0xfeU)));
    value += 'b';
    std::string expected{"\"a "};
    expected.push_back(static_cast<char>(static_cast<std::uint8_t>(0xfeU)));
    expected += "b\"";

    EXPECT_EQ(log_format::escape_value(value), expected);
}

} // namespace
