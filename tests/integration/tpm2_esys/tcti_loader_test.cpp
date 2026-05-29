#include "src/adapters/tpm2_esys/context/tcti_loader.h"
#include "src/adapters/tpm2_esys/support/log_events.h"

#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

namespace {

std::string swtpm_tcti()
{
    const char* const configured = std::getenv("TPMKIT_SWTPM_TCTI");
    if (configured != nullptr) {
        return std::string{configured};
    }

    return "tabrmd:bus_type=system";
}

TEST(tcti_loader_integration, loads_configured_simulator_tcti)
{
    // Verifies the TCTI loader opens a configured simulator TCTI.

    const tpmkit::tcti_string_config config{swtpm_tcti()};

    auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_NE(result->get(), nullptr);
}

TEST(tcti_loader_integration, invalid_device_config_returns_error_without_leaking_loader_context)
{
    // Verifies failed loader initialization releases caller-owned TCTI storage.

    const tpmkit::tcti_string_config config{"device:/dev/nonexistent"};

    auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().category == tpmkit::error_category::resource_error ||
                result.error().category == tpmkit::error_category::backend_error ||
                result.error().category == tpmkit::error_category::input_error);
}

TEST(tcti_loader_integration, successful_load_emits_no_error_records)
{
    // Verifies the logger port is wired and the success path produces no error-level records.

    tpmkit::testing::recording_logger log;
    const tpmkit::tcti_string_config config{swtpm_tcti()};

    auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    const auto records = log.snapshot();
    const bool has_error = std::any_of(records.begin(), records.end(), [](const auto& r) {
        return r.level == tpmkit::log_level::error;
    });
    EXPECT_FALSE(has_error);
}

TEST(tcti_loader_integration, failed_load_emits_tss_error_record_with_operation_field)
{
    // Verifies an error-level tss_error record with operation=tcti_init is emitted on failure.

    tpmkit::testing::recording_logger log;
    const tpmkit::tcti_string_config config{"device:/dev/nonexistent"};

    auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    const auto records = log.snapshot();
    const auto it = std::find_if(records.begin(), records.end(), [](const auto& r) {
        return r.level == tpmkit::log_level::error &&
               r.message == tpmkit::detail::esys::events::tss_error;
    });
    ASSERT_NE(it, records.end()) << "expected a tss_error log record";
    const bool has_operation = std::any_of(it->fields.begin(), it->fields.end(), [](const auto& f) {
        return f.first == tpmkit::detail::esys::events::fields::operation &&
               f.second == "tcti_init";
    });
    EXPECT_TRUE(has_operation);
}

} // namespace
