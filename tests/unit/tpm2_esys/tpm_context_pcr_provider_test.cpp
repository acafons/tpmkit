#include <tpmkit/pcr/observer.h>
#include <tpmkit/testing/fake_tcti.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm_context.h>

#include "src/adapters/tpm2_esys/log_events.h"

#include <gsl/span>
#include <gtest/gtest.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

#if defined(__GNUC__) || defined(__clang__)
// TSS constants in public headers intentionally expand through C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

namespace events = tpmkit::detail::esys::events;
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

void append_u8(std::vector<std::uint8_t>& out, const std::uint8_t value)
{
    out.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& out, const std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_u32(std::vector<std::uint8_t>& out, const std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void append_header(std::vector<std::uint8_t>& out, const std::uint16_t tag,
                   const std::uint32_t size, const std::uint32_t rc)
{
    append_u16(out, tag);
    append_u32(out, size);
    append_u32(out, rc);
}

void append_empty_auth_response(std::vector<std::uint8_t>& out)
{
    append_u16(out, 0U);
    append_u8(out, 0U);
    append_u16(out, 0U);
}

void append_sha256_selection(std::vector<std::uint8_t>& out, const std::uint8_t pcr)
{
    append_u32(out, 1U);
    append_u16(out, TPM2_ALG_SHA256);
    append_u8(out, TPM2_PCR_SELECT_MAX);
    std::array<std::uint8_t, TPM2_PCR_SELECT_MAX> select{};
    select[pcr / 8U] = static_cast<std::uint8_t>(1U << (pcr % 8U));
    out.insert(out.end(), select.begin(), select.end());
}

std::string field_value(const tpmkit::testing::log_record& record, const std::string_view key)
{
    for (const auto& field : record.fields) {
        if (field.first == key) {
            return field.second;
        }
    }

    return {};
}

tpmkit::tpm_context_config owned_config(tpmkit::testing::fake_tcti& fake,
                                        std::shared_ptr<tpmkit::logger> log = nullptr)
{
    tpmkit::tpm_context_config config;
    config.tcti =
        tpmkit::tcti_owned_handle{std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(
            fake.handle(), finalize_tcti_handle)};
    config.startup = startup_mode::skip;
    config.log = std::move(log);
    return config;
}

std::vector<std::uint8_t> digest_bytes(const std::uint8_t seed)
{
    std::vector<std::uint8_t> bytes(TPM2_SHA256_DIGEST_SIZE);
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }

    return bytes;
}

tpmkit::pcr::digest_value sha256_digest(const std::uint8_t seed)
{
    return tpmkit::pcr::digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(seed)};
}

std::vector<std::uint8_t> extend_success_response()
{
    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS, 19U, TSS2_RC_SUCCESS);
    append_u32(response, 0U);
    append_empty_auth_response(response);
    return response;
}

std::vector<std::uint8_t> read_success_response(const std::uint32_t update_counter,
                                                const std::uint8_t pcr,
                                                const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, update_counter);
    append_sha256_selection(parameters, pcr);
    append_u32(parameters, 1U);
    append_u16(parameters, static_cast<std::uint16_t>(digest.size()));
    parameters.insert(parameters.end(), digest.begin(), digest.end());

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS,
                  static_cast<std::uint32_t>(10U + parameters.size()), TSS2_RC_SUCCESS);
    response.insert(response.end(), parameters.begin(), parameters.end());
    return response;
}

TEST(tpm_context_pcr_provider, creates_provider_for_valid_context)
{
    // Verifies a valid TPM context creates a PCR provider.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());

    auto provider = context->create_pcr_provider();

    ASSERT_TRUE(provider.has_value());
    EXPECT_NE(provider.value(), nullptr);
}

TEST(tpm_context_pcr_provider, returns_resource_error_for_invalid_context)
{
    // Verifies moved-from TPM contexts are rejected before provider construction.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
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

    tpmkit::testing::fake_tcti fake;
    const auto digest = digest_bytes(0x10U);
    fake.push_response(read_success_response(5U, 16U, digest));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider = context->create_pcr_provider();
    ASSERT_TRUE(provider.has_value());

    const auto result = provider.value()->read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->update_counter, 5U);
    ASSERT_EQ(result->values.size(), 1U);
    EXPECT_EQ(result->values.front().index, tpmkit::pcr::index::debug);
    EXPECT_EQ(result->values.front().digest.digest(), digest);
}

TEST(tpm_context_pcr_provider, provider_extends_with_null_observer_and_default_logger)
{
    // Verifies null observer and omitted logger select no-op behavior.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider = context->create_pcr_provider(nullptr);
    ASSERT_TRUE(provider.has_value());
    const tpmkit::pcr::digest_value digest = sha256_digest(0x20U);

    const auto result =
        provider.value()->extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    EXPECT_TRUE(result.has_value());
}

TEST(tpm_context_pcr_provider, provider_notifies_non_null_observer)
{
    // Verifies non-null observer is passed into the ESYS PCR provider.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider = context->create_pcr_provider(&observer);
    ASSERT_TRUE(provider.has_value());
    const tpmkit::pcr::digest_value digest = sha256_digest(0x30U);

    const auto result =
        provider.value()->extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 1U);
    EXPECT_EQ(observer.extended_index, 16U);
    ASSERT_EQ(observer.extended_digests.size(), 1U);
    EXPECT_EQ(observer.extended_digests.front(), digest);
}

TEST(tpm_context_pcr_provider, provider_uses_context_logger)
{
    // Verifies provider operations use the logger configured on the TPM context.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider = context->create_pcr_provider(nullptr);
    ASSERT_TRUE(provider.has_value());
    const tpmkit::pcr::digest_value digest = sha256_digest(0x40U);

    const auto result =
        provider.value()->extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_extend_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace
