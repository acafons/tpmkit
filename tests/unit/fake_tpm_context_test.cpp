#include <tpmkit/testing/fake_tpm_context.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>
#include <variant>

namespace {

using startup_mode = tpmkit::tpm_context_config::startup_mode;

TEST(fake_tpm_context, default_config_with_empty_tcti_string_returns_input_error)
{
    const tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create({});

    ASSERT_FALSE(created.has_value());
    EXPECT_EQ(created.error().category, tpmkit::error_category::input_error);
}

TEST(fake_tpm_context, valid_string_config_returns_success)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"mssim:host=localhost,port=2321"};
    config.startup = startup_mode::skip;
    config.log = nullptr;

    tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create(std::move(config));

    EXPECT_TRUE(created.has_value());
}

TEST(fake_tpm_context, successful_context_exposes_last_config_for_introspection)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"mssim:host=localhost,port=2321"};
    config.startup = startup_mode::skip;

    tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create(std::move(config));

    ASSERT_TRUE(created.has_value());
    const tpmkit::testing::fake_tpm_context context = std::move(created).value();
    EXPECT_EQ(context.last_config().startup, startup_mode::skip);
    ASSERT_TRUE(std::holds_alternative<tpmkit::tcti_string_config>(context.last_config().tcti));
    EXPECT_EQ(
        std::get<tpmkit::tcti_string_config>(context.last_config().tcti).config,
        "mssim:host=localhost,port=2321");
}

TEST(fake_tpm_context, is_move_only)
{
    static_assert(
        std::is_move_constructible<tpmkit::testing::fake_tpm_context>::value,
        "fake_tpm_context must be move constructible");
    static_assert(
        std::is_move_assignable<tpmkit::testing::fake_tpm_context>::value,
        "fake_tpm_context must be move assignable");
    static_assert(
        !std::is_copy_constructible<tpmkit::testing::fake_tpm_context>::value,
        "fake_tpm_context must not be copy constructible");
    static_assert(
        !std::is_copy_assignable<tpmkit::testing::fake_tpm_context>::value,
        "fake_tpm_context must not be copy assignable");

    EXPECT_TRUE(std::is_move_constructible<tpmkit::testing::fake_tpm_context>::value);
    EXPECT_TRUE(std::is_move_assignable<tpmkit::testing::fake_tpm_context>::value);
    EXPECT_FALSE(std::is_copy_constructible<tpmkit::testing::fake_tpm_context>::value);
    EXPECT_FALSE(std::is_copy_assignable<tpmkit::testing::fake_tpm_context>::value);
}

TEST(fake_tpm_context, move_construction_preserves_moved_to_introspection)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"mssim:host=localhost,port=2321"};
    config.startup = startup_mode::state;

    tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create(std::move(config));
    ASSERT_TRUE(created.has_value());

    tpmkit::testing::fake_tpm_context original = std::move(created).value();
    tpmkit::testing::fake_tpm_context moved{std::move(original)};

    EXPECT_EQ(moved.last_config().startup, startup_mode::state);
    ASSERT_TRUE(std::holds_alternative<tpmkit::tcti_string_config>(moved.last_config().tcti));
    EXPECT_EQ(
        std::get<tpmkit::tcti_string_config>(moved.last_config().tcti).config,
        "mssim:host=localhost,port=2321");
    EXPECT_NO_THROW((void)original.last_config());
}

TEST(fake_tpm_context, null_owned_handle_returns_input_error)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_owned_handle{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(nullptr, nullptr)};

    const tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create(std::move(config));

    ASSERT_FALSE(created.has_value());
    EXPECT_EQ(created.error().category, tpmkit::error_category::input_error);
}

} // namespace
