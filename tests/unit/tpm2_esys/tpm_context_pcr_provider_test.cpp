#include "esys_fake_api.h"

#include <tpmkit/pcr/observer.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm_context.h>

#include "src/adapters/tpm2_esys/context/impl.h"
#include "src/adapters/tpm2_esys/support/log_events.h"

#include <gsl/span>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace events = tpmkit::detail::esys::events;
namespace fake = tpmkit::testing::esys;
using startup_mode = tpmkit::tpm_context_config::startup_mode;

class recording_observer final : public tpmkit::pcr::observer {
public:
    void on_event(tpmkit::pcr::index, gsl::span<const std::uint8_t>,
                  const tpmkit::pcr::event_result&) noexcept final
    {
        ++event_calls;
    }

    void on_extend(const tpmkit::pcr::index index,
                   const gsl::span<const tpmkit::pcr::digest_value> digests) noexcept final
    {
        extended_index = index.value();
        extended_digests.assign(digests.begin(), digests.end());
        ++extend_calls;
    }

    std::size_t event_calls{0U};
    std::size_t extend_calls{0U};
    std::uint8_t extended_index{0U};
    std::vector<tpmkit::pcr::digest_value> extended_digests;
};

tpmkit::outcome<tpmkit::tpm_context>
create_owned_context(fake::fake_esys_state& state, std::shared_ptr<tpmkit::logger> log = nullptr)
{
    return tpmkit::detail::esys::create_context_from_owned_tcti(
        fake::owned_tcti(state), startup_mode::skip, std::move(log), fake::fake_api());
}

void expect_provider_log_record(const tpmkit::testing::log_record& record,
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

TEST(tpm_context_pcr_provider, creates_provider_for_valid_context)
{
    // Verifies a valid TPM context creates a PCR provider.

    fake::fake_esys_state state;
    auto context = create_owned_context(state);
    ASSERT_TRUE(context.has_value());

    auto provider = context->create_pcr_provider();

    ASSERT_TRUE(provider.has_value());
    EXPECT_NE(*provider, nullptr);
}

TEST(tpm_context_pcr_provider, returns_resource_error_for_invalid_context)
{
    // Verifies moved-from TPM contexts are rejected before provider construction.

    fake::fake_esys_state state;
    auto context = create_owned_context(state);
    ASSERT_TRUE(context.has_value());
    tpmkit::tpm_context moved_context{*std::move(context)};

    auto provider = context->create_pcr_provider();

    static_cast<void>(moved_context);
    ASSERT_FALSE(provider.has_value());
    EXPECT_EQ(provider.error().category, tpmkit::error_category::resource_error);
}

TEST(tpm_context_pcr_provider, provider_reads_through_port_interface)
{
    // Verifies a context-created provider works through the pcr::provider interface.

    fake::fake_esys_state state;
    const auto digest = fake::digest_bytes(0x10U);
    state.read_update_counter = 5U;
    state.read_actual_selection = fake::sha256_selection(tpmkit::pcr::index::debug.value());
    state.read_values = fake::read_values(digest);
    auto context = create_owned_context(state);
    ASSERT_TRUE(context.has_value());
    auto provider = context->create_pcr_provider();
    ASSERT_TRUE(provider.has_value());
    auto& pcr_provider = *provider.value();

    const auto result = pcr_provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->update_counter, 5U);
    ASSERT_EQ(result->values.size(), 1U);
    EXPECT_EQ(result->values.front().index, tpmkit::pcr::index::debug);
    EXPECT_EQ(result->values.front().digest.digest(), digest);
    ASSERT_EQ(state.read_calls.size(), 1U);
    EXPECT_EQ(state.read_calls.front().selection.pcrSelections[0U].hash, TPM2_ALG_SHA256);
}

TEST(tpm_context_pcr_provider, provider_extends_with_null_observer_and_default_logger)
{
    // Verifies null observer and omitted logger select no-op behavior.

    fake::fake_esys_state state;
    auto context = create_owned_context(state);
    ASSERT_TRUE(context.has_value());
    auto provider = context->create_pcr_provider(nullptr);
    ASSERT_TRUE(provider.has_value());
    auto& pcr_provider = *provider.value();
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x20U);

    const auto result = pcr_provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    EXPECT_TRUE(result.has_value());
    ASSERT_EQ(state.extend_calls.size(), 1U);
    EXPECT_EQ(state.extend_calls.front().digests.count, 1U);
}

TEST(tpm_context_pcr_provider, provider_notifies_non_null_observer)
{
    // Verifies non-null observer is passed into the ESYS PCR provider.

    fake::fake_esys_state state;
    auto context = create_owned_context(state);
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider = context->create_pcr_provider(&observer);
    ASSERT_TRUE(provider.has_value());
    auto& pcr_provider = *provider.value();
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x30U);

    const auto result = pcr_provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 1U);
    EXPECT_EQ(observer.extended_index, 16U);
    ASSERT_EQ(observer.extended_digests.size(), 1U);
    EXPECT_EQ(observer.extended_digests.front(), digest);
}

TEST(tpm_context_pcr_provider, provider_uses_context_logger)
{
    // Verifies provider operations use the logger configured on the TPM context.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = create_owned_context(state, log);
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider = context->create_pcr_provider(nullptr);
    ASSERT_TRUE(provider.has_value());
    auto& pcr_provider = *provider.value();
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x40U);

    const auto result = pcr_provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_provider_log_record(records.front(), events::pcr_extend_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::bank_count), "1");
}
