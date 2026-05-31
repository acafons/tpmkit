#include <tpmkit/testing/fake_tcti.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm2_esys/owned_tcti_context.h>
#include <tpmkit/tpm_context.h>

#include "src/adapters/tpm2_esys/context/impl.h"
#include "src/adapters/tpm2_esys/support/log_events.h"

#include <gtest/gtest.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace events = tpmkit::detail::esys::events;

using startup_mode = tpmkit::tpm_context_config::startup_mode;

TSS2_TCTI_CONTEXT_COMMON_V1* common(TSS2_TCTI_CONTEXT* const context) noexcept
{
    return reinterpret_cast<TSS2_TCTI_CONTEXT_COMMON_V1*>(context);
}

void finalize_tcti_handle(TSS2_TCTI_CONTEXT* const context) noexcept
{
    if (context == nullptr) {
        return;
    }

    TSS2_TCTI_CONTEXT_COMMON_V1* const callbacks = common(context);
    if (callbacks->finalize != nullptr) {
        callbacks->finalize(context);
    }
}

std::vector<std::uint8_t> startup_response(const std::uint32_t rc)
{
    const std::uint32_t size = 10U;
    return {
        0x80U,
        0x01U,
        static_cast<std::uint8_t>((size >> 24U) & 0xffU),
        static_cast<std::uint8_t>((size >> 16U) & 0xffU),
        static_cast<std::uint8_t>((size >> 8U) & 0xffU),
        static_cast<std::uint8_t>(size & 0xffU),
        static_cast<std::uint8_t>((rc >> 24U) & 0xffU),
        static_cast<std::uint8_t>((rc >> 16U) & 0xffU),
        static_cast<std::uint8_t>((rc >> 8U) & 0xffU),
        static_cast<std::uint8_t>(rc & 0xffU),
    };
}

tpmkit::tpm2_esys::owned_tcti_context owned_tcti(tpmkit::testing::fake_tcti& fake)
{
    return tpmkit::tpm2_esys::owned_tcti_context{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(fake.handle(),
                                                                         finalize_tcti_handle)};
}

tpmkit::outcome<tpmkit::tpm_context>
create_owned_context(tpmkit::testing::fake_tcti& fake, const startup_mode mode,
                     std::shared_ptr<tpmkit::logger> log = nullptr)
{
    return tpmkit::tpm2_esys::create_context(owned_tcti(fake), mode, std::move(log));
}

std::string field_value(const tpmkit::testing::log_record& record, const std::string_view key)
{
    for (const auto& field : record.fields) {
        if (std::string_view{field.first} == key) {
            return field.second;
        }
    }

    return {};
}

bool contains_field_value(const std::vector<tpmkit::testing::log_record>& records,
                          const std::string_view forbidden)
{
    for (const auto& record : records) {
        for (const auto& field : record.fields) {
            if (field.second.find(forbidden) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

} // namespace

TEST(tpm_context_lifecycle, create_with_owned_tcti_clear_starts_and_finalizes)
{
    // Verifies clear startup uses the owned TCTI and finalizes on destruction.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(startup_response(TSS2_RC_SUCCESS));

    {
        auto result = create_owned_context(fake, startup_mode::clear);

        ASSERT_TRUE(result.has_value());
        auto provider = result->create_pcr_provider();
        ASSERT_TRUE(provider.has_value());
        EXPECT_NE(*provider, nullptr);
        EXPECT_EQ(fake.transmits_observed(), 1U);
        EXPECT_EQ(fake.finalizes_observed(), 0U);
    }

    EXPECT_EQ(fake.finalizes_observed(), 1U);
}

TEST(tpm_context_lifecycle, create_with_skip_does_not_transmit_startup)
{
    // Verifies skip startup avoids transmitting a TPM startup command.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    tpmkit::testing::fake_tcti fake;

    {
        auto result = create_owned_context(fake, startup_mode::skip, log);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(fake.transmits_observed(), 0U);
    }

    EXPECT_EQ(fake.finalizes_observed(), 1U);

    const auto records = log->snapshot();
    bool found_completion = false;
    for (const auto& record : records) {
        if (field_value(record, events::fields::event) ==
                std::string{events::startup_completed.name} &&
            field_value(record, events::fields::result) == "skipped") {
            found_completion = true;
        }
    }

    EXPECT_TRUE(found_completion);
}

TEST(tpm_context_lifecycle, invalid_startup_mode_returns_input_error_without_transmit)
{
    // Verifies invalid startup modes are rejected before issuing TPM commands.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    tpmkit::testing::fake_tcti fake;

    const auto result = create_owned_context(fake, static_cast<startup_mode>(99), log);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(fake.transmits_observed(), 0U);
    EXPECT_TRUE(log->snapshot().empty());
}

TEST(tpm_context_lifecycle, string_tcti_overload_rejects_invalid_config_shape)
{
    // Verifies the string TCTI overload forwards validation through the config path.

    const auto result = tpmkit::tpm_context::create(std::string{"swtpm"});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tpm_context_lifecycle, startup_initialize_response_is_success)
{
    // Verifies TPM2_RC_INITIALIZE is treated as successful startup.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    tpmkit::testing::fake_tcti fake;
    fake.push_response(startup_response(TPM2_RC_INITIALIZE));

    {
        auto result = create_owned_context(fake, startup_mode::clear, log);

        ASSERT_TRUE(result.has_value());
    }

    const auto records = log->snapshot();
    ASSERT_GE(records.size(), 5U);
    bool found = false;
    for (const auto& record : records) {
        if (field_value(record, events::fields::event) ==
            std::string{events::startup_completed.name}) {
            found = true;
        }
    }

    EXPECT_TRUE(found);
    EXPECT_EQ(fake.finalizes_observed(), 1U);
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
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(nullptr,
                                                                         finalize_tcti_handle)};

    const auto result = tpmkit::tpm2_esys::create_context(std::move(tcti));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
}

TEST(tpm_context_lifecycle, null_owned_handle_deleter_returns_input_error_without_finalize)
{
    // Verifies a missing owned-handle deleter is rejected without finalizing.

    tpmkit::testing::fake_tcti fake;
    tpmkit::tpm2_esys::owned_tcti_context tcti{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(fake.handle(), nullptr)};

    const auto result = tpmkit::tpm2_esys::create_context(std::move(tcti));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(fake.finalizes_observed(), 0U);
}

TEST(tpm_context_lifecycle, startup_failure_finalizes_tcti_before_returning_error)
{
    // Verifies startup failures release the owned TCTI before returning.

    tpmkit::testing::fake_tcti fake;
    fake.push_failure(TSS2_TCTI_RC_IO_ERROR);

    const auto result = create_owned_context(fake, startup_mode::clear);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    EXPECT_EQ(fake.finalizes_observed(), 1U);
}

TEST(tpm_context_lifecycle, startup_transmit_failure_finalizes_tcti_before_returning_error)
{
    // Verifies startup transmit failures release the owned TCTI before returning.

    tpmkit::testing::fake_tcti fake;
    fake.push_transmit_failure(TSS2_TCTI_RC_IO_ERROR);

    const auto result = create_owned_context(fake, startup_mode::clear);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    EXPECT_EQ(fake.finalizes_observed(), 1U);
}

TEST(tpm_context_lifecycle, happy_path_emits_documented_lifecycle_sequence)
{
    // Verifies the successful lifecycle emits the documented event sequence.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    tpmkit::testing::fake_tcti fake;
    fake.push_response(startup_response(TSS2_RC_SUCCESS));

    {
        auto result = create_owned_context(fake, startup_mode::clear, log);

        ASSERT_TRUE(result.has_value());
    }

    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 6U);
    EXPECT_EQ(field_value(records[0], events::fields::event),
              std::string{events::tcti_configuring.name});
    EXPECT_EQ(field_value(records[1], events::fields::event),
              std::string{events::tcti_configured.name});
    EXPECT_EQ(field_value(records[2], events::fields::event),
              std::string{events::esys_initialized.name});
    EXPECT_EQ(field_value(records[3], events::fields::event),
              std::string{events::startup_invoked.name});
    EXPECT_EQ(field_value(records[4], events::fields::event),
              std::string{events::startup_completed.name});
    EXPECT_EQ(field_value(records[5], events::fields::event), std::string{events::finalized.name});
}

TEST(tpm_context_lifecycle, string_tcti_failure_logs_only_sanitized_module_name)
{
    // Verifies failing string TCTI logs do not leak config details.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();

    const auto result = tpmkit::tpm_context::create(
        std::string{"not_a_real_tcti:secret_socket=/tmp/secret"}, startup_mode::clear, log);

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_FALSE(records.empty());
    EXPECT_EQ(field_value(records.front(), events::fields::tcti_name), "<custom>");
    EXPECT_FALSE(contains_field_value(records, "secret_socket"));
    EXPECT_FALSE(contains_field_value(records, "/tmp/secret"));
}

TEST(tpm_context_lifecycle, path_like_tcti_name_is_not_logged)
{
    // Verifies path-like TCTI names are redacted from logs.

    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"/tmp/secret:socket=/tmp/secret"};
    config.log = log;

    const auto result = tpmkit::tpm_context::create(std::move(config));

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_FALSE(records.empty());
    EXPECT_EQ(field_value(records.front(), events::fields::tcti_name), "<custom>");
    EXPECT_FALSE(contains_field_value(records, "/tmp/secret"));
}

TEST(tpm_context_lifecycle, independent_contexts_are_thread_compatible)
{
    // Verifies independent TPM contexts can be created concurrently.

    std::array<bool, 2U> succeeded{{false, false}};
    std::array<std::size_t, 2U> transmits{{0U, 0U}};

    auto run = [&](const std::size_t index) {
        tpmkit::testing::fake_tcti fake;
        fake.push_response(startup_response(TSS2_RC_SUCCESS));
        auto result = create_owned_context(fake, startup_mode::clear);
        succeeded[index] = result.has_value();
        transmits[index] = fake.transmits_observed();
    };

    std::thread first{run, 0U};
    std::thread second{run, 1U};
    first.join();
    second.join();

    EXPECT_TRUE(succeeded[0]);
    EXPECT_TRUE(succeeded[1]);
    EXPECT_EQ(transmits[0], 1U);
    EXPECT_EQ(transmits[1], 1U);
}
