#include <tpmkit/tpm_context.h>

#include <gtest/gtest.h>

#include <type_traits>

TEST(tpm_context_config, defaults_to_empty_string_tcti_and_clear_startup)
{
    // Verifies the default TPM context config uses the documented defaults.

    const tpmkit::tpm_context_config config;

    EXPECT_TRUE(config.tcti.config.empty());
    EXPECT_EQ(config.startup, tpmkit::tpm_context_config::startup_mode::clear);
    EXPECT_EQ(config.log, nullptr);
}

TEST(tpm_context_config, stores_string_tcti_config)
{
    // Verifies the backend-neutral config stores only string TCTI selection.

    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"mssim:host=localhost,port=2321"};

    EXPECT_EQ(config.tcti.config, "mssim:host=localhost,port=2321");
}

TEST(tpm_context, is_move_only)
{
    // Verifies tpm_context preserves move-only ownership semantics.

    EXPECT_TRUE(std::is_move_constructible<tpmkit::tpm_context>::value);
    EXPECT_TRUE(std::is_move_assignable<tpmkit::tpm_context>::value);
    EXPECT_FALSE(std::is_copy_constructible<tpmkit::tpm_context>::value);
    EXPECT_FALSE(std::is_copy_assignable<tpmkit::tpm_context>::value);
}
