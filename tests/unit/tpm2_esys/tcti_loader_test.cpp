#include "src/adapters/tpm2_esys/context/tcti_loader.h"

#include "src/adapters/tpm2_esys/support/log_events.h"

#include <tpmkit/logging/logger.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct log_record {
    tpmkit::log_level level;
    std::vector<std::pair<std::string, std::string>> fields;
    std::string message;
};

class recording_logger final : public tpmkit::logger {
public:
    void log(const tpmkit::log_level level, const std::string_view message,
             const gsl::span<const tpmkit::log_field> fields) noexcept final
    {
        std::vector<std::pair<std::string, std::string>> copied_fields;
        copied_fields.reserve(fields.size());
        for (const auto& field : fields) {
            copied_fields.emplace_back(field.key, field.value);
        }

        records.push_back(log_record{level, std::move(copied_fields), std::string{message}});
    }

    std::vector<log_record> records;
};

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

TEST(tcti_loader, rejects_empty_config_without_tss_code_in_message)
{
    // Verifies empty TCTI configs fail before exposing TSS details.

    const tpmkit::tcti_string_config config{""};

    const auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
}

TEST(tcti_loader, rejects_whitespace_only_config)
{
    // Verifies whitespace-only TCTI configs are rejected as input errors.

    const tpmkit::tcti_string_config config{" \t\n "};

    const auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tcti_loader, rejects_missing_colon_shape_before_adapter_boundary_log)
{
    // Verifies malformed TCTI shape is rejected before adapter logging.

    recording_logger log;
    const tpmkit::tcti_string_config config{"swtpm"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.records.empty());
}

TEST(tcti_loader, rejects_missing_name_before_colon)
{
    // Verifies TCTI configs with no module name are rejected.

    const tpmkit::tcti_string_config config{":socket=/tmp/tpm.sock"};

    const auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tcti_loader, does_not_log_client_side_validation_failures)
{
    // Verifies client-side validation failures do not emit adapter logs.

    recording_logger log;
    const tpmkit::tcti_string_config config{" :socket=/tmp/tpm.sock"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.records.empty());
}

TEST(tcti_loader, unknown_tcti_name_returns_input_error_without_tss_code)
{
    // Verifies unknown TCTI names return sanitized input errors.

    recording_logger log;
    const tpmkit::tcti_string_config config{"not_a_real_tcti:config"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
    ASSERT_EQ(log.records.size(), 1U);
    EXPECT_EQ(log.records.front().level, tpmkit::log_level::error);
    EXPECT_EQ(log.records.front().message,
              std::string{tpmkit::detail::esys::events::tss_error.message});
    EXPECT_EQ(log.records.front().fields.size(), 8U);
    const auto* event =
        find_field(log.records.front().fields, tpmkit::detail::esys::events::fields::event);
    const auto* operation =
        find_field(log.records.front().fields, tpmkit::detail::esys::events::fields::operation);
    const auto* backend_description =
        find_field(log.records.front().fields,
                   tpmkit::detail::esys::events::fields::backend_error_description);
    ASSERT_NE(event, nullptr);
    ASSERT_NE(operation, nullptr);
    ASSERT_NE(backend_description, nullptr);
    EXPECT_EQ(event->second, std::string{tpmkit::detail::esys::events::tss_error.name});
    EXPECT_EQ(operation->second, "tcti_init");
    EXPECT_FALSE(backend_description->second.empty());
}

} // namespace
