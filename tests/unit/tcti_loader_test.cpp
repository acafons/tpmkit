#include "src/adapters/tpm2_esys/tcti_loader.h"

#include <tpmkit/logger.h>

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
    void log(
        const tpmkit::log_level level,
        const std::string_view message,
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

TEST(tcti_loader, rejects_empty_config_without_tss_code_in_message)
{
    const tpmkit::tcti_string_config config{""};

    const auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
}

TEST(tcti_loader, rejects_whitespace_only_config)
{
    const tpmkit::tcti_string_config config{" \t\n "};

    const auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tcti_loader, rejects_missing_colon_shape_before_adapter_boundary_log)
{
    recording_logger log;
    const tpmkit::tcti_string_config config{"swtpm"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.records.empty());
}

TEST(tcti_loader, does_not_log_client_side_validation_failures)
{
    recording_logger log;
    const tpmkit::tcti_string_config config{" :socket=/tmp/tpm.sock"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(log.records.empty());
}

TEST(tcti_loader, logs_tss_failure_without_returning_tss_code)
{
    recording_logger log;
    const tpmkit::tcti_string_config config{"not_a_real_tcti:config"};

    const auto result = tpmkit::detail::esys::load_tcti(config, &log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
    EXPECT_FALSE(contains_numeric_tss_code(result.error().message));
    ASSERT_EQ(log.records.size(), 1U);
    EXPECT_EQ(log.records.front().level, tpmkit::log_level::error);
    EXPECT_EQ(log.records.front().message, "tpm.context.tss_error");
    EXPECT_EQ(log.records.front().fields.size(), 3U);
}

} // namespace
