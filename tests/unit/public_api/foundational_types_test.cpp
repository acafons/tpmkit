#include <tpmkit/exception.h>
#include <tpmkit/logging/noop_logger.h>
#include <tpmkit/result.h>

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {

TEST(foundational_types, outcome_holds_value)
{
    // Verifies outcome exposes its contained success value.

    const tpmkit::outcome<int> result{42};

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(foundational_types, outcome_holds_error)
{
    // Verifies outcome exposes its contained error.

    const tpmkit::outcome<int> result{tl::unexpect,
                                      tpmkit::error{tpmkit::error_category::input_error, "bad"}};

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(result.error().message, "bad");
}

TEST(foundational_types, errors_with_same_fields_are_observably_equivalent)
{
    // Verifies errors with matching fields compare through their observable fields.

    const tpmkit::error first{tpmkit::error_category::backend_error, "failed"};
    const tpmkit::error second{tpmkit::error_category::backend_error, "failed"};

    EXPECT_EQ(first.category, second.category);
    EXPECT_EQ(first.message, second.message);
}

TEST(foundational_types, tpmkit_error_preserves_message)
{
    // Verifies tpmkit_error exposes its construction message.

    const tpmkit::tpmkit_error error{"x"};

    EXPECT_STREQ(error.what(), "x");
}

TEST(foundational_types, tpmkit_error_rejects_null_c_string_message)
{
    // Verifies tpmkit_error validates nullable C-string input.

    try {
        const tpmkit::tpmkit_error error{nullptr};
        static_cast<void>(error);
        FAIL() << "Expected tpmkit_error for null message";
    } catch (const tpmkit::tpmkit_error& error) {
        EXPECT_STREQ(error.what(), "tpmkit_error message must not be null");
    }
}

TEST(foundational_types, noop_logger_satisfies_logger_port)
{
    // Verifies noop_logger can be called through the logger port.

    tpmkit::noop_logger noop;
    tpmkit::logger* const log = &noop;
    const std::array<tpmkit::log_field, 1> fields{{{"event", "test.noop"}}};

    EXPECT_NO_THROW(log->log(tpmkit::log_level::info, "noop logger called",
                             gsl::span<const tpmkit::log_field>(fields)));
}

} // namespace
