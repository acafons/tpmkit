#include "esys_fake_api.h"

#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm_context.h>

#include "src/adapters/tpm2_esys/context/impl.h"
#include "src/adapters/tpm2_esys/support/log_events.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace events = tpmkit::detail::esys::events;
namespace fake = tpmkit::testing::esys;

using startup_mode = tpmkit::tpm_context_config::startup_mode;

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
        *size = sizeof(fake::fake_esys_state);
    }
    return TSS2_TCTI_RC_BAD_SEQUENCE;
}

const char* fake_decode_error(TSS2_RC)
{
    return "fake tcti loader error";
}

const tpmkit::detail::esys::tcti_loader_api& failing_loader_api()
{
    static const tpmkit::detail::esys::tcti_loader_api api{
        fake_get_info_failure,
        fake_free_info,
        fake_loader_init,
        fake_decode_error,
    };

    return api;
}

tpmkit::outcome<tpmkit::tpm_context>
create_owned_context(fake::fake_esys_state& state, const startup_mode mode,
                     std::shared_ptr<tpmkit::logger> log = nullptr)
{
    return tpmkit::detail::esys::create_context_with_api(fake::owned_tcti(state), mode,
                                                         std::move(log), fake::fake_api());
}

tpmkit::outcome<tpmkit::tpm_context>
create_string_context(std::string config, std::shared_ptr<tpmkit::logger> log = nullptr)
{
    tpmkit::tpm_context_config context_config;
    context_config.tcti = tpmkit::tcti_string_config{std::move(config)};
    context_config.log = std::move(log);
    return tpmkit::detail::esys::create_context_with_apis(std::move(context_config),
                                                          failing_loader_api(), fake::fake_api());
}

void expect_lifecycle_success_record(const tpmkit::testing::log_record& record,
                                     const events::event_descriptor& descriptor)
{
    EXPECT_EQ(record.level, tpmkit::log_level::info);
    EXPECT_EQ(record.message, std::string{descriptor.message});
    EXPECT_EQ(fake::field_value(record, events::fields::event), std::string{descriptor.name});
    EXPECT_EQ(fake::field_value(record, events::fields::component),
              std::string{events::component_tpm2_esys});
    EXPECT_EQ(fake::field_value(record, events::fields::outcome),
              std::string{events::values::success});
}

} // namespace

TEST(tpm_context_lifecycle, create_with_owned_tcti_clear_starts_and_finalizes)
{
    // Verifies clear startup uses the owned TCTI and finalizes on destruction.

    fake::fake_esys_state state;

    {
        auto result = create_owned_context(state, startup_mode::clear);

        ASSERT_TRUE(result.has_value());
        auto provider = result->create_pcr_provider();
        ASSERT_TRUE(provider.has_value());
        EXPECT_NE(*provider, nullptr);
        EXPECT_EQ(state.initializes, 1U);
        EXPECT_EQ(state.startups, 1U);
        EXPECT_EQ(state.startup_type, TPM2_SU_CLEAR);
        EXPECT_EQ(state.esys_finalizes, 0U);
        EXPECT_EQ(state.tcti_finalizes, 0U);
    }

    EXPECT_EQ(state.esys_finalizes, 1U);
    EXPECT_EQ(state.tcti_finalizes, 1U);
}

TEST(tpm_context_lifecycle, create_with_skip_does_not_transmit_startup)
{
    // Verifies skip startup avoids issuing a TPM startup command.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    fake::fake_esys_state state;

    {
        auto result = create_owned_context(state, startup_mode::skip, log);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(state.startups, 0U);
    }

    EXPECT_EQ(state.esys_finalizes, 1U);
    EXPECT_EQ(state.tcti_finalizes, 1U);
    const auto records = log->snapshot();
    ASSERT_GE(records.size(), 1U);
    bool found_completion = false;
    for (const auto& record : records) {
        if (fake::field_value(record, events::fields::event) ==
                std::string{events::startup_completed.name} &&
            fake::field_value(record, events::fields::result) == "skipped") {
            found_completion = true;
        }
    }

    EXPECT_TRUE(found_completion);
}

TEST(tpm_context_lifecycle, invalid_startup_mode_returns_input_error_without_backend_calls)
{
    // Verifies invalid startup modes are rejected before issuing ESYS calls.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    fake::fake_esys_state state;

    const auto result = create_owned_context(state, static_cast<startup_mode>(99), log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(state.initializes, 0U);
    EXPECT_EQ(state.startups, 0U);
    EXPECT_TRUE(log->snapshot().empty());
}

TEST(tpm_context_lifecycle, string_tcti_overload_rejects_invalid_config_shape)
{
    // Verifies the string TCTI overload forwards validation through the config path.

    const auto result = create_string_context("swtpm");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tpm_context_lifecycle, startup_initialize_response_is_success)
{
    // Verifies TPM2_RC_INITIALIZE is treated as successful startup.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    fake::fake_esys_state state;
    state.startup_rc = TPM2_RC_INITIALIZE;

    {
        auto result = create_owned_context(state, startup_mode::clear, log);

        ASSERT_TRUE(result.has_value());
    }

    const auto records = log->snapshot();
    ASSERT_GE(records.size(), 1U);
    bool found = false;
    for (const auto& record : records) {
        if (fake::field_value(record, events::fields::event) ==
                std::string{events::startup_completed.name} &&
            fake::field_value(record, events::fields::result) == "already_initialized") {
            found = true;
        }
    }

    EXPECT_TRUE(found);
    EXPECT_EQ(state.esys_finalizes, 1U);
}

TEST(tpm_context_lifecycle, raw_startup_initialize_rc_maps_to_already_initialized_result)
{
    // Verifies raw startup helpers classify TPM2_RC_INITIALIZE consistently.

    EXPECT_TRUE(tpmkit::detail::esys::is_startup_already_initialized(TPM2_RC_INITIALIZE));
    EXPECT_EQ(tpmkit::detail::esys::startup_result_field(TPM2_RC_INITIALIZE),
              std::string_view{"already_initialized"});
}

TEST(tpm_context_lifecycle, null_owned_handle_returns_input_error)
{
    // Verifies a null owned TCTI handle is rejected during context creation.

    tpmkit::tpm2_esys::owned_tcti_context tcti{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(
            nullptr, fake::finalize_tcti_handle)};

    const auto result = tpmkit::detail::esys::create_context_with_api(
        std::move(tcti), startup_mode::clear, nullptr, fake::fake_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tpm_context_lifecycle, null_owned_handle_deleter_returns_input_error_without_finalize)
{
    // Verifies a missing owned-handle deleter is rejected without finalizing.

    fake::fake_esys_state state;
    tpmkit::tpm2_esys::owned_tcti_context tcti{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(fake::tcti_handle(state),
                                                                         nullptr)};

    const auto result = tpmkit::detail::esys::create_context_with_api(
        std::move(tcti), startup_mode::clear, nullptr, fake::fake_api());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(state.tcti_finalizes, 0U);
}

TEST(tpm_context_lifecycle, initialize_failure_finalizes_tcti_before_returning_error)
{
    // Verifies ESYS initialization failures release the owned TCTI before returning.

    fake::fake_esys_state state;
    state.initialize_rc = TSS2_TCTI_RC_IO_ERROR;

    const auto result = create_owned_context(state, startup_mode::clear);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    EXPECT_EQ(state.esys_finalizes, 0U);
    EXPECT_EQ(state.tcti_finalizes, 1U);
}

TEST(tpm_context_lifecycle, startup_failure_finalizes_contexts_before_returning_error)
{
    // Verifies startup failures release ESYS and TCTI contexts before returning.

    fake::fake_esys_state state;
    state.startup_rc = TSS2_TCTI_RC_IO_ERROR;

    const auto result = create_owned_context(state, startup_mode::clear);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    EXPECT_EQ(state.esys_finalizes, 1U);
    EXPECT_EQ(state.tcti_finalizes, 1U);
}

TEST(tpm_context_lifecycle, happy_path_emits_documented_lifecycle_sequence)
{
    // Verifies the successful lifecycle emits the documented event sequence.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    fake::fake_esys_state state;

    {
        auto result = create_owned_context(state, startup_mode::clear, log);

        ASSERT_TRUE(result.has_value());
    }

    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 6U);
    expect_lifecycle_success_record(records[0U], events::tcti_configuring);
    expect_lifecycle_success_record(records[1U], events::tcti_configured);
    expect_lifecycle_success_record(records[2U], events::esys_initialized);
    expect_lifecycle_success_record(records[3U], events::startup_invoked);
    expect_lifecycle_success_record(records[4U], events::startup_completed);
    expect_lifecycle_success_record(records[5U], events::finalized);
    EXPECT_EQ(fake::field_value(records[0U], events::fields::tcti_kind), "owned_handle");
    EXPECT_EQ(fake::field_value(records[2U], events::fields::abi_version), "1.2.3.4");
    EXPECT_EQ(fake::field_value(records[4U], events::fields::startup_mode), "clear");
    EXPECT_EQ(fake::field_value(records[4U], events::fields::result), "ok");
}

TEST(tpm_context_lifecycle, string_tcti_failure_logs_only_sanitized_module_name)
{
    // Verifies failing string TCTI logs do not leak config details.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();

    const auto result = create_string_context("not_a_real_tcti:secret_socket=/tmp/secret", log);

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_FALSE(records.empty());
    expect_lifecycle_success_record(records.front(), events::tcti_configuring);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::tcti_name), "<custom>");
    EXPECT_FALSE(fake::contains_field_value(records, "secret_socket"));
    EXPECT_FALSE(fake::contains_field_value(records, "/tmp/secret"));
}

TEST(tpm_context_lifecycle, path_like_tcti_name_is_not_logged)
{
    // Verifies path-like TCTI names are redacted from logs.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();

    const auto result = create_string_context("/tmp/secret:socket=/tmp/secret", log);

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_FALSE(records.empty());
    expect_lifecycle_success_record(records.front(), events::tcti_configuring);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::tcti_name), "<custom>");
    EXPECT_FALSE(fake::contains_field_value(records, "/tmp/secret"));
}
