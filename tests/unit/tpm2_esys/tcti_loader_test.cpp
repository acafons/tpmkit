#include "src/adapters/tpm2_esys/context/tcti_loader.h"

#include "src/adapters/tpm2_esys/support/log_events.h"

#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

TSS2_RC fake_get_info_failure(const char*, TSS2_TCTI_INFO** const info)
{
    if (info != nullptr) {
        *info = nullptr;
    }
    return TSS2_TCTI_RC_NOT_SUPPORTED;
}

void fake_free_info(TSS2_TCTI_INFO** const info) noexcept
{
    if (info != nullptr) {
        *info = nullptr;
    }
}

TSS2_RC fake_loader_init(TSS2_TCTI_CONTEXT*, std::size_t* const size, const char*)
{
    if (size != nullptr) {
        *size = 0U;
    }
    return TSS2_TCTI_RC_BAD_SEQUENCE;
}

const char* fake_decode_error(TSS2_RC)
{
    return "fake tcti loader error";
}

const tpmkit::detail::tpm2_esys::tcti_loader_api& failing_loader_api()
{
    static const tpmkit::detail::tpm2_esys::tcti_loader_api api{
        fake_get_info_failure,
        fake_free_info,
        fake_loader_init,
        fake_decode_error,
    };

    return api;
}

[[nodiscard]] bool contains_numeric_tss_code(const std::string& message)
{
    return message.find("0x") != std::string::npos || message.find("TSS") != std::string::npos;
}

[[nodiscard]] const std::pair<std::string, std::string>*
find_field(const std::vector<std::pair<std::string, std::string>>& fields,
           const std::string_view key)
{
    for (const auto& field : fields) {
        if (field.first == key) {
            return &field;
        }
    }

    return nullptr;
}

} // namespace

TEST(tcti_loader, rejects_empty_config_without_tss_code_in_message)
{
    // Verifies empty TCTI configs fail before exposing TSS details.

    const tpmkit::tcti_string_config config{""};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, nullptr, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
}

TEST(tcti_loader, rejects_whitespace_only_config)
{
    // Verifies whitespace-only TCTI configs are rejected as input errors.

    const tpmkit::tcti_string_config config{" \t\n "};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, nullptr, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tcti_loader, rejects_missing_colon_shape_before_adapter_boundary_log)
{
    // Verifies malformed TCTI shape is rejected before adapter logging.

    tpmkit::testing::recording_logger log;
    const tpmkit::tcti_string_config config{"swtpm"};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, &log, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.snapshot().empty());
}

TEST(tcti_loader, rejects_missing_name_before_colon)
{
    // Verifies TCTI configs with no module name are rejected.

    const tpmkit::tcti_string_config config{":socket=/tmp/tpm.sock"};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, nullptr, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tcti_loader, does_not_log_client_side_validation_failures)
{
    // Verifies client-side validation failures do not emit adapter logs.

    tpmkit::testing::recording_logger log;
    const tpmkit::tcti_string_config config{" :socket=/tmp/tpm.sock"};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, &log, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.snapshot().empty());
}

TEST(tcti_loader, unknown_tcti_name_returns_input_error_without_tss_code)
{
    // Verifies unknown TCTI names return sanitized input errors.

    tpmkit::testing::recording_logger log;
    const tpmkit::tcti_string_config config{"not_a_real_tcti:config"};

    const auto result = tpmkit::detail::tpm2_esys::load_tcti(config, &log, failing_loader_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
    const auto records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, tpmkit::log_level::error);
    EXPECT_EQ(records.front().message,
              std::string{tpmkit::detail::tpm2_esys::events::tss_error.message});
    EXPECT_EQ(records.front().fields.size(), 8U);
    const auto* event =
        find_field(records.front().fields, tpmkit::detail::tpm2_esys::events::fields::event);
    const auto* component =
        find_field(records.front().fields, tpmkit::detail::tpm2_esys::events::fields::component);
    const auto* outcome =
        find_field(records.front().fields, tpmkit::detail::tpm2_esys::events::fields::outcome);
    const auto* operation =
        find_field(records.front().fields, tpmkit::detail::tpm2_esys::events::fields::operation);
    const auto* backend_description =
        find_field(records.front().fields,
                   tpmkit::detail::tpm2_esys::events::fields::backend_error_description);
    ASSERT_NE(event, nullptr);
    ASSERT_NE(component, nullptr);
    ASSERT_NE(outcome, nullptr);
    ASSERT_NE(operation, nullptr);
    ASSERT_NE(backend_description, nullptr);
    EXPECT_EQ(event->second, std::string{tpmkit::detail::tpm2_esys::events::tss_error.name});
    EXPECT_EQ(component->second,
              std::string{tpmkit::detail::tpm2_esys::events::component_tpm2_esys});
    EXPECT_EQ(outcome->second, std::string{tpmkit::detail::tpm2_esys::events::values::failure});
    EXPECT_EQ(operation->second, "tcti_init");
    EXPECT_EQ(backend_description->second, "fake tcti loader error");
}
