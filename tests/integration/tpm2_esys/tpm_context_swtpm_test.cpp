#include <tpmkit/tpm_context.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <utility>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TPMKIT_TEST_ASAN 1
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
#define TPMKIT_TEST_ASAN 1
#endif

namespace {

bool swtpm_integration_enabled() noexcept
{
    const char* const enabled = std::getenv("TPMKIT_RUN_SWTPM_INTEGRATION");
    return enabled != nullptr && std::string{enabled} == "1";
}

std::string swtpm_tcti()
{
    const char* const configured = std::getenv("TPMKIT_SWTPM_TCTI");
    if (configured != nullptr) {
        return std::string{configured};
    }

    return "tabrmd:bus_type=system";
}

tpmkit::tpm_context_config string_config(std::string tcti,
                                         const tpmkit::tpm_context_config::startup_mode startup)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{std::move(tcti)};
    config.startup = startup;
    return config;
}

} // namespace

TEST(tpm_context_swtpm, constructs_and_tears_down_against_simulator)
{
    if (!swtpm_integration_enabled()) {
        GTEST_SKIP() << "Set TPMKIT_RUN_SWTPM_INTEGRATION=1 after starting "
                        "scripts/tpm-start.sh.";
    }

    auto result = tpmkit::tpm_context::create(
        string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_NE(result.value().esys_handle(), nullptr);
}

TEST(tpm_context_swtpm, clear_startup_is_idempotent_against_simulator)
{
    if (!swtpm_integration_enabled()) {
        GTEST_SKIP() << "Set TPMKIT_RUN_SWTPM_INTEGRATION=1 after starting "
                        "scripts/tpm-start.sh.";
    }

    {
        auto first = tpmkit::tpm_context::create(
            string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));
        ASSERT_TRUE(first.has_value()) << first.error().message;
    }

    auto second = tpmkit::tpm_context::create(
        string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));

    ASSERT_TRUE(second.has_value()) << second.error().message;
}

TEST(tpm_context_swtpm, invalid_device_tcti_returns_domain_error)
{
#if defined(TPMKIT_TEST_ASAN)
    GTEST_SKIP() << "tpm2-tss leaks a loader allocation on failed device TCTI "
                    "init under ASan.";
#endif

    auto result = tpmkit::tpm_context::create(
        string_config("device:/dev/nonexistent", tpmkit::tpm_context_config::startup_mode::skip));

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().category == tpmkit::error_category::resource_error ||
                result.error().category == tpmkit::error_category::backend_error);
}
