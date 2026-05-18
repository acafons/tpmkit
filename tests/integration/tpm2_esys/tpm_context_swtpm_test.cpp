#include <tpmkit/tpm_context.h>

#ifdef TPMKIT_HAS_SPDLOG
#include <tpmkit/spdlog_logger.h>

#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <sstream>
#endif

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

namespace {

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
    // Verifies a real TPM context constructs and tears down against swtpm.

    auto result = tpmkit::tpm_context::create(
        string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_NE(result.value().esys_handle(), nullptr);
}

TEST(tpm_context_swtpm, clear_startup_is_idempotent_against_simulator)
{
    // Verifies repeated clear startup succeeds against the simulator.

    {
        auto first = tpmkit::tpm_context::create(
            string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));
        ASSERT_TRUE(first.has_value()) << first.error().message;
    }

    auto second = tpmkit::tpm_context::create(
        string_config(swtpm_tcti(), tpmkit::tpm_context_config::startup_mode::clear));

    ASSERT_TRUE(second.has_value()) << second.error().message;
}

#ifdef TPMKIT_HAS_SPDLOG
TEST(tpm_context_swtpm, create_with_spdlog_adapter_produces_lifecycle_records)
{
    // Verifies spdlog_logger wired through tpm_context_config receives lifecycle events.

    std::ostringstream captured;
    auto sink = std::make_shared<::spdlog::sinks::ostream_sink_mt>(captured);
    sink->set_level(::spdlog::level::trace);
    sink->set_pattern("%v");
    auto inner = std::make_shared<::spdlog::logger>("tpmkit", sink);
    inner->set_level(::spdlog::level::trace);
    auto log = std::make_shared<tpmkit::spdlog_logger>(std::move(inner));

    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{swtpm_tcti()};
    config.startup = tpmkit::tpm_context_config::startup_mode::clear;
    config.log = log;

    auto result = tpmkit::tpm_context::create(std::move(config));
    log->flush();

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_FALSE(captured.str().empty());
}
#endif

TEST(tpm_context_swtpm, invalid_device_tcti_returns_domain_error)
{
    // Verifies an invalid device TCTI returns a domain error category.

    auto result = tpmkit::tpm_context::create(
        string_config("device:/dev/nonexistent", tpmkit::tpm_context_config::startup_mode::skip));

    ASSERT_FALSE(result.has_value());
    // resource_error / backend_error when the device TCTI module is installed but the
    // path does not exist; input_error when the device TCTI module is absent entirely.
    EXPECT_TRUE(result.error().category == tpmkit::error_category::resource_error ||
                result.error().category == tpmkit::error_category::backend_error ||
                result.error().category == tpmkit::error_category::input_error);
}
