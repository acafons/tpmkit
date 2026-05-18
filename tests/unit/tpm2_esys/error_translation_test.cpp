#include "src/adapters/tpm2_esys/error_translation.h"

#include "src/adapters/tpm2_esys/log_events.h"

#include <tpmkit/logger.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tpm2_types.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct log_record {
    tpmkit::log_level level;
    std::string message;
    std::vector<std::pair<std::string, std::string>> fields;
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

        records.push_back(log_record{level, std::string{message}, std::move(copied_fields)});
    }

    std::vector<log_record> records;
};

struct mapping_case {
    const char* name;
    TSS2_RC rc;
    tpmkit::error_category category;
    std::string_view layer;
};

constexpr auto input = tpmkit::error_category::input_error;
constexpr auto security = tpmkit::error_category::security_failure;
constexpr auto resource = tpmkit::error_category::resource_error;
constexpr auto backend = tpmkit::error_category::backend_error;
namespace events = tpmkit::detail::esys::events;

[[nodiscard]] bool contains_disallowed_message_detail(const std::string& message)
{
    if (message.find("0x") != std::string::npos || message.find("TSS") != std::string::npos) {
        return true;
    }

    if (message.find("RC") != std::string::npos || message.find("ESYS") != std::string::npos) {
        return true;
    }

    return message.find_first_of("0123456789") != std::string::npos;
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

const std::vector<mapping_case>& documented_cases()
{
#if defined(__GNUC__) || defined(__clang__)
// TSS return-code constants expand through C-style casts in the system headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    static const std::vector<mapping_case> cases{
        {"TSS2_ESYS_RC_GENERAL_FAILURE", TSS2_ESYS_RC_GENERAL_FAILURE, backend, "esapi"},
        {"TSS2_ESYS_RC_NOT_IMPLEMENTED", TSS2_ESYS_RC_NOT_IMPLEMENTED, backend, "esapi"},
        {"TSS2_ESYS_RC_ABI_MISMATCH", TSS2_ESYS_RC_ABI_MISMATCH, backend, "esapi"},
        {"TSS2_ESYS_RC_BAD_REFERENCE", TSS2_ESYS_RC_BAD_REFERENCE, input, "esapi"},
        {"TSS2_ESYS_RC_INSUFFICIENT_BUFFER", TSS2_ESYS_RC_INSUFFICIENT_BUFFER, input, "esapi"},
        {"TSS2_ESYS_RC_BAD_SEQUENCE", TSS2_ESYS_RC_BAD_SEQUENCE, backend, "esapi"},
        {"TSS2_ESYS_RC_INVALID_SESSIONS", TSS2_ESYS_RC_INVALID_SESSIONS, input, "esapi"},
        {"TSS2_ESYS_RC_TRY_AGAIN", TSS2_ESYS_RC_TRY_AGAIN, resource, "esapi"},
        {"TSS2_ESYS_RC_IO_ERROR", TSS2_ESYS_RC_IO_ERROR, resource, "esapi"},
        {"TSS2_ESYS_RC_BAD_VALUE", TSS2_ESYS_RC_BAD_VALUE, input, "esapi"},
        {"TSS2_ESYS_RC_NO_DECRYPT_PARAM", TSS2_ESYS_RC_NO_DECRYPT_PARAM, input, "esapi"},
        {"TSS2_ESYS_RC_NO_ENCRYPT_PARAM", TSS2_ESYS_RC_NO_ENCRYPT_PARAM, input, "esapi"},
        {"TSS2_ESYS_RC_BAD_SIZE", TSS2_ESYS_RC_BAD_SIZE, input, "esapi"},
        {"TSS2_ESYS_RC_MALFORMED_RESPONSE", TSS2_ESYS_RC_MALFORMED_RESPONSE, backend, "esapi"},
        {"TSS2_ESYS_RC_INSUFFICIENT_CONTEXT", TSS2_ESYS_RC_INSUFFICIENT_CONTEXT, backend, "esapi"},
        {
            "TSS2_ESYS_RC_INSUFFICIENT_RESPONSE",
            TSS2_ESYS_RC_INSUFFICIENT_RESPONSE,
            backend,
            "esapi",
        },
        {"TSS2_ESYS_RC_INCOMPATIBLE_TCTI", TSS2_ESYS_RC_INCOMPATIBLE_TCTI, resource, "esapi"},
        {"TSS2_ESYS_RC_BAD_TCTI_STRUCTURE", TSS2_ESYS_RC_BAD_TCTI_STRUCTURE, backend, "esapi"},
        {"TSS2_ESYS_RC_MEMORY", TSS2_ESYS_RC_MEMORY, resource, "esapi"},
        {"TSS2_ESYS_RC_BAD_TR", TSS2_ESYS_RC_BAD_TR, input, "esapi"},
        {
            "TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS",
            TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS,
            input,
            "esapi",
        },
        {
            "TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS",
            TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS,
            input,
            "esapi",
        },
        {"TSS2_ESYS_RC_NOT_SUPPORTED", TSS2_ESYS_RC_NOT_SUPPORTED, backend, "esapi"},
        {"TSS2_ESYS_RC_RSP_AUTH_FAILED", TSS2_ESYS_RC_RSP_AUTH_FAILED, security, "esapi"},
        {"TSS2_ESYS_RC_CALLBACK_NULL", TSS2_ESYS_RC_CALLBACK_NULL, backend, "esapi"},
        {"TSS2_TCTI_RC_GENERAL_FAILURE", TSS2_TCTI_RC_GENERAL_FAILURE, backend, "tcti"},
        {"TSS2_TCTI_RC_NOT_IMPLEMENTED", TSS2_TCTI_RC_NOT_IMPLEMENTED, backend, "tcti"},
        {"TSS2_TCTI_RC_BAD_CONTEXT", TSS2_TCTI_RC_BAD_CONTEXT, backend, "tcti"},
        {"TSS2_TCTI_RC_ABI_MISMATCH", TSS2_TCTI_RC_ABI_MISMATCH, backend, "tcti"},
        {"TSS2_TCTI_RC_BAD_REFERENCE", TSS2_TCTI_RC_BAD_REFERENCE, input, "tcti"},
        {"TSS2_TCTI_RC_INSUFFICIENT_BUFFER", TSS2_TCTI_RC_INSUFFICIENT_BUFFER, input, "tcti"},
        {"TSS2_TCTI_RC_BAD_SEQUENCE", TSS2_TCTI_RC_BAD_SEQUENCE, backend, "tcti"},
        {"TSS2_TCTI_RC_NO_CONNECTION", TSS2_TCTI_RC_NO_CONNECTION, resource, "tcti"},
        {"TSS2_TCTI_RC_TRY_AGAIN", TSS2_TCTI_RC_TRY_AGAIN, resource, "tcti"},
        {"TSS2_TCTI_RC_IO_ERROR", TSS2_TCTI_RC_IO_ERROR, resource, "tcti"},
        {"TSS2_TCTI_RC_BAD_VALUE", TSS2_TCTI_RC_BAD_VALUE, input, "tcti"},
        {"TSS2_TCTI_RC_NOT_PERMITTED", TSS2_TCTI_RC_NOT_PERMITTED, backend, "tcti"},
        {"TSS2_TCTI_RC_MALFORMED_RESPONSE", TSS2_TCTI_RC_MALFORMED_RESPONSE, backend, "tcti"},
        {"TSS2_TCTI_RC_NOT_SUPPORTED", TSS2_TCTI_RC_NOT_SUPPORTED, backend, "tcti"},
    };
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    return cases;
}

TEST(error_translation, success_returns_value_without_logging)
{
    // Verifies successful TSS return codes produce no log record.

    recording_logger log;

    const auto result =
        tpmkit::detail::esys::translate_tss_rc(TSS2_RC_SUCCESS, "esys_initialize", &log);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(log.records.empty());
}

TEST(error_translation, maps_every_documented_esapi_and_tcti_rc)
{
    // Verifies documented ESAPI and TCTI return codes map to categories.

    for (const auto& test_case : documented_cases()) {
        const auto result =
            tpmkit::detail::esys::translate_tss_rc(test_case.rc, "esys_initialize", nullptr);

        ASSERT_FALSE(result.has_value()) << test_case.name;
        EXPECT_EQ(result.error().category, test_case.category) << test_case.name;
        EXPECT_FALSE(contains_disallowed_message_detail(result.error().message)) << test_case.name;
    }
}

TEST(error_translation, maps_explicit_task_examples)
{
    // Verifies representative task return-code examples map correctly.

#if defined(__GNUC__) || defined(__clang__)
// TSS return-code constants expand through C-style casts in the system headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    const std::vector<mapping_case> cases{
        {"TSS2_BASE_RC_BAD_VALUE at ESAPI", TSS2_ESYS_RC_BAD_VALUE, input, "esapi"},
        {"TSS2_BASE_RC_IO_ERROR at TCTI", TSS2_TCTI_RC_IO_ERROR, resource, "tcti"},
        {"TSS2_ESYS_RC_ABI_MISMATCH", TSS2_ESYS_RC_ABI_MISMATCH, backend, "esapi"},
        {"TPM2_RC_INITIALIZE", static_cast<TSS2_RC>(TPM2_RC_INITIALIZE), backend, "tpm"},
        {"unknown", 0xDEADBEEFU, backend, "unknown"},
    };
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    for (const auto& test_case : cases) {
        const auto result =
            tpmkit::detail::esys::translate_tss_rc(test_case.rc, "esys_startup", nullptr);

        ASSERT_FALSE(result.has_value()) << test_case.name;
        EXPECT_EQ(result.error().category, test_case.category) << test_case.name;
        EXPECT_FALSE(contains_disallowed_message_detail(result.error().message)) << test_case.name;
    }
}

TEST(error_translation, logs_non_success_rc_with_schema_fields)
{
    // Verifies failed TSS return codes emit the documented schema fields.

#if defined(__GNUC__) || defined(__clang__)
// TSS return-code constants expand through C-style casts in the system headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    constexpr TSS2_RC rc = TSS2_TCTI_RC_IO_ERROR;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    recording_logger log;

    const auto result = tpmkit::detail::esys::translate_tss_rc(rc, "tcti_init", &log);

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(log.records.size(), 1U);
    const auto& record = log.records.front();
    EXPECT_EQ(record.level, tpmkit::log_level::error);
    EXPECT_EQ(record.message, events::tss_error);
    ASSERT_EQ(record.fields.size(), 3U);

    const auto* operation = find_field(record.fields, events::fields::operation);
    const auto* rc_hex = find_field(record.fields, events::fields::tss_rc_hex);
    const auto* layer = find_field(record.fields, events::fields::tss_layer);

    ASSERT_NE(operation, nullptr);
    ASSERT_NE(rc_hex, nullptr);
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(operation->second, "tcti_init");
    EXPECT_EQ(rc_hex->second, "0x000a000a");
    EXPECT_EQ(layer->second, "tcti");
}

TEST(error_translation, logs_representative_tss_layer_names)
{
    // Verifies logged TSS layer names are stable for representative codes.

#if defined(__GNUC__) || defined(__clang__)
// TSS return-code constants expand through C-style casts in the system headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    const std::vector<mapping_case> cases{
        {"tpm", static_cast<TSS2_RC>(TPM2_RC_INITIALIZE), backend, "tpm"},
        {"esapi", TSS2_ESYS_RC_BAD_VALUE, input, "esapi"},
        {"sys", TSS2_SYS_RC_BAD_VALUE, backend, "sys"},
        {"mu", TSS2_MU_RC_BAD_VALUE, backend, "mu"},
        {"unknown", 0xDEADBEEFU, backend, "unknown"},
    };
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    for (const auto& test_case : cases) {
        recording_logger log;

        const auto result =
            tpmkit::detail::esys::translate_tss_rc(test_case.rc, "esys_startup", &log);

        ASSERT_FALSE(result.has_value()) << test_case.name;
        EXPECT_EQ(result.error().category, test_case.category) << test_case.name;
        ASSERT_EQ(log.records.size(), 1U) << test_case.name;

        const auto* layer = find_field(log.records.front().fields, events::fields::tss_layer);
        ASSERT_NE(layer, nullptr) << test_case.name;
        EXPECT_EQ(layer->second, test_case.layer) << test_case.name;
    }
}

} // namespace
