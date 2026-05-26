#include <tpmkit/logging/noop_logger.h>
#include <tpmkit/testing/fake_tcti.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm_context.h>

#include "src/adapters/tpm2_esys/esys_pcr_provider.h"
#include "src/adapters/tpm2_esys/log_events.h"

#include <gtest/gtest.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <algorithm>
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

tpmkit::logger& default_log()
{
    return tpmkit::noop_logger::instance();
}

class recording_observer final : public tpmkit::pcr::observer {
public:
    void on_extend(const tpmkit::pcr::index index,
                   const gsl::span<const tpmkit::pcr::digest_value> digests) noexcept final
    {
        extended_index = index.value();
        extended_digests.assign(digests.begin(), digests.end());
        ++extend_calls;
    }

    void on_event(const tpmkit::pcr::index index, const gsl::span<const std::uint8_t> event_data,
                  const tpmkit::pcr::event_result& result) noexcept final
    {
        event_index = index.value();
        event_bytes.assign(event_data.begin(), event_data.end());
        event_result = result;
        ++event_calls;
    }

    std::size_t event_calls{0U};
    std::size_t extend_calls{0U};
    std::uint8_t event_index{0U};
    std::uint8_t extended_index{0U};
    std::vector<std::uint8_t> event_bytes;
    std::vector<tpmkit::pcr::digest_value> extended_digests;
    tpmkit::pcr::event_result event_result;
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

void append_sha256_digest_value(std::vector<std::uint8_t>& out,
                                const std::vector<std::uint8_t>& digest)
{
    append_u16(out, TPM2_ALG_SHA256);
    out.insert(out.end(), digest.begin(), digest.end());
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3U]);
}

std::uint16_t read_u16(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8U) |
                                      static_cast<std::uint16_t>(bytes[offset + 1U]));
}

std::uint8_t read_u8(const std::vector<std::uint8_t>& bytes, const std::size_t offset)
{
    return bytes.at(offset);
}

std::vector<std::uint8_t> error_response(const std::uint32_t rc)
{
    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS, 10U, rc);
    return response;
}

std::vector<std::uint8_t> extend_success_response()
{
    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS, 19U, TSS2_RC_SUCCESS);
    append_u32(response, 0U);
    append_empty_auth_response(response);
    return response;
}

std::vector<std::uint8_t> simple_session_success_response()
{
    return extend_success_response();
}

std::vector<std::uint8_t> event_success_response(const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, 1U);
    append_sha256_digest_value(parameters, digest);

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS,
                  static_cast<std::uint32_t>(10U + 4U + parameters.size() + 5U), TSS2_RC_SUCCESS);
    append_u32(response, static_cast<std::uint32_t>(parameters.size()));
    response.insert(response.end(), parameters.begin(), parameters.end());
    append_empty_auth_response(response);
    return response;
}

std::vector<std::uint8_t> allocate_success_response(const bool allocation_success,
                                                    const std::uint32_t max_pcr,
                                                    const std::uint32_t size_needed,
                                                    const std::uint32_t size_available)
{
    std::vector<std::uint8_t> parameters;
    append_u8(parameters, allocation_success ? TPM2_YES : TPM2_NO);
    append_u32(parameters, max_pcr);
    append_u32(parameters, size_needed);
    append_u32(parameters, size_available);

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS,
                  static_cast<std::uint32_t>(10U + 4U + parameters.size() + 5U), TSS2_RC_SUCCESS);
    append_u32(response, static_cast<std::uint32_t>(parameters.size()));
    response.insert(response.end(), parameters.begin(), parameters.end());
    append_empty_auth_response(response);
    return response;
}

std::vector<std::uint8_t>
event_success_response_with_algorithm(const std::uint16_t algorithm,
                                      const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, 1U);
    append_u16(parameters, algorithm);
    parameters.insert(parameters.end(), digest.begin(), digest.end());

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS,
                  static_cast<std::uint32_t>(10U + 4U + parameters.size() + 5U), TSS2_RC_SUCCESS);
    append_u32(response, static_cast<std::uint32_t>(parameters.size()));
    response.insert(response.end(), parameters.begin(), parameters.end());
    append_empty_auth_response(response);
    return response;
}

std::vector<std::uint8_t> read_empty_selection_response()
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, 3U);
    append_u32(parameters, 0U);
    append_u32(parameters, 0U);

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS,
                  static_cast<std::uint32_t>(10U + parameters.size()), TSS2_RC_SUCCESS);
    response.insert(response.end(), parameters.begin(), parameters.end());
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

std::vector<std::uint8_t> read_bad_digest_size_response(const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, 1U);
    append_sha256_selection(parameters, 16U);
    append_u32(parameters, 1U);
    append_u16(parameters, static_cast<std::uint16_t>(digest.size()));
    parameters.insert(parameters.end(), digest.begin(), digest.end());

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS,
                  static_cast<std::uint32_t>(10U + parameters.size()), TSS2_RC_SUCCESS);
    response.insert(response.end(), parameters.begin(), parameters.end());
    return response;
}

std::vector<std::uint8_t>
read_digest_count_mismatch_response(const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> parameters;
    append_u32(parameters, 1U);
    append_u32(parameters, 1U);
    append_u16(parameters, TPM2_ALG_SHA256);
    append_u8(parameters, TPM2_PCR_SELECT_MAX);
    std::array<std::uint8_t, TPM2_PCR_SELECT_MAX> select{};
    select[2U] = 0x03U;
    parameters.insert(parameters.end(), select.begin(), select.end());
    append_u32(parameters, 1U);
    append_u16(parameters, static_cast<std::uint16_t>(digest.size()));
    parameters.insert(parameters.end(), digest.begin(), digest.end());

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS,
                  static_cast<std::uint32_t>(10U + parameters.size()), TSS2_RC_SUCCESS);
    response.insert(response.end(), parameters.begin(), parameters.end());
    return response;
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

std::unique_ptr<tpmkit::pcr::provider>
require_pcr_provider(tpmkit::tpm_context& context, tpmkit::pcr::observer* const observer = nullptr)
{
    auto provider = context.create_pcr_provider(observer);
    if (!provider.has_value()) {
        ADD_FAILURE() << provider.error().message;
        return nullptr;
    }

    return std::move(provider).value();
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

bool contains_bytes(const std::vector<std::uint8_t>& haystack,
                    const std::vector<std::uint8_t>& needle)
{
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) !=
           haystack.end();
}

std::size_t command_parameter_offset(const std::vector<std::uint8_t>& command)
{
    const std::size_t auth_size_offset = 14U;
    return auth_size_offset + 4U + read_u32(command, auth_size_offset);
}

std::uint32_t allocation_selection_count(const std::vector<std::uint8_t>& command)
{
    return read_u32(command, command_parameter_offset(command));
}

bool has_standard_pcr_bank_selection(const std::vector<std::uint8_t>& command,
                                     const std::uint16_t algorithm)
{
    constexpr std::uint8_t standard_pcr_select_size = 3U;
    std::size_t offset = command_parameter_offset(command);
    const std::uint32_t count = read_u32(command, offset);
    offset += 4U;

    for (std::uint32_t index = 0U; index < count; ++index) {
        const std::uint16_t actual_algorithm = read_u16(command, offset);
        const std::uint8_t select_size = read_u8(command, offset + 2U);
        offset += 3U;

        bool all_selected = select_size == standard_pcr_select_size;
        for (std::uint8_t byte_index = 0U; byte_index < select_size; ++byte_index) {
            all_selected = all_selected && read_u8(command, offset + byte_index) == 0xffU;
        }

        if (actual_algorithm == algorithm && all_selected) {
            return true;
        }

        offset += select_size;
    }

    return false;
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

TEST(esys_pcr_provider, read_translates_selection_and_returns_result)
{
    // Verifies PCR read translates selection bytes and returns domain values.

    tpmkit::testing::fake_tcti fake;
    const auto digest = digest_bytes(0x10U);
    fake.push_response(read_success_response(7U, 16U, digest));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().update_counter, 7U);
    ASSERT_EQ(result.value().values.size(), 1U);
    EXPECT_EQ(result.value().values.front().index, tpmkit::pcr::index::debug);
    EXPECT_EQ(result.value().values.front().digest.digest(), digest);
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Read);
    EXPECT_TRUE(contains_bytes(commands.front(), {0x00U, 0x0bU, 0x03U, 0x00U, 0x00U, 0x01U}));
}

TEST(esys_pcr_provider, read_uses_four_byte_selection_for_high_pcr_index)
{
    // Verifies PCR indices above 23 are represented without widening standard selections.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(read_empty_selection_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index{31U}}});

    ASSERT_TRUE(result.has_value());
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Read);
    EXPECT_TRUE(
        contains_bytes(commands.front(), {0x00U, 0x0bU, 0x04U, 0x00U, 0x00U, 0x00U, 0x80U}));
}

TEST(esys_pcr_provider, read_returns_resource_error_for_transport_failure)
{
    // Verifies PCR read maps transport failures to resource errors.

    tpmkit::testing::fake_tcti fake;
    fake.push_transmit_failure(TSS2_TCTI_RC_IO_ERROR);
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, read_returns_backend_error_for_unexpected_tpm_code)
{
    // Verifies unknown TPM failures remain backend errors.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_INITIALIZE));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_rejects_digest_count_mismatch_from_tpm)
{
    // Verifies PCR read rejects TPM responses whose digest count does not match selection.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(read_digest_count_mismatch_response(digest_bytes(0x12U)));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(tpmkit::pcr::selection{
        tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug, tpmkit::pcr::index::drtm_17}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_emits_success_log_fields)
{
    // Verifies PCR read success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(read_success_response(1U, 16U, digest_bytes(0x20U)));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_read_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::bank), "sha256");
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_count), "1");
}

TEST(esys_pcr_provider, read_accepts_empty_actual_selection)
{
    // Verifies PCR read accepts a TPM response with no selected PCRs.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(read_empty_selection_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256});

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().actual_selection.indices().empty());
    EXPECT_TRUE(result.value().values.empty());
}

TEST(esys_pcr_provider, read_rejects_bad_digest_size_from_tpm)
{
    // Verifies malformed PCR read digest sizes return backend errors.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(read_bad_digest_size_response(std::vector<std::uint8_t>(31U, 0xaaU)));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_rejects_missing_esys_context)
{
    // Verifies PCR read requires an ESYS context.

    tpmkit::detail::esys::esys_pcr_provider provider{nullptr, default_log(), nullptr};

    const auto result = provider.read(tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, extend_translates_digest_and_calls_observer)
{
    // Verifies PCR extend sends digest values and notifies the observer after success.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider_owner = require_pcr_provider(context.value(), &observer);
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const tpmkit::pcr::digest_value digest = sha256_digest(0x30U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 1U);
    EXPECT_EQ(observer.extended_index, 16U);
    ASSERT_EQ(observer.extended_digests.size(), 1U);
    EXPECT_EQ(observer.extended_digests.front(), digest);
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Extend);
    EXPECT_TRUE(contains_bytes(commands.front(), digest.digest()));
}

TEST(esys_pcr_provider, extend_rejects_missing_esys_context)
{
    // Verifies PCR extend requires an ESYS context.

    tpmkit::detail::esys::esys_pcr_provider provider{nullptr, default_log(), nullptr};
    const tpmkit::pcr::digest_value digest = sha256_digest(0x38U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, extend_rejects_too_many_digests)
{
    // Verifies PCR extend validates the TPM digest-list capacity.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::vector<tpmkit::pcr::digest_value> digests;
    digests.reserve(TPM2_NUM_PCR_BANKS + 1U);
    for (std::size_t index = 0U; index < TPM2_NUM_PCR_BANKS + 1U; ++index) {
        digests.push_back(sha256_digest(static_cast<std::uint8_t>(0x39U + index)));
    }

    const auto result = provider.extend(tpmkit::pcr::index::debug, digests);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(fake.transmits_observed(), 0U);
}

TEST(esys_pcr_provider, extend_rejects_empty_digest_list_before_dispatch)
{
    // Verifies PCR extend requires at least one digest and sends no TPM command on empty input.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto result =
        provider.extend(tpmkit::pcr::index::debug, gsl::span<const tpmkit::pcr::digest_value>{});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(fake.transmitted_commands().empty());
}

TEST(esys_pcr_provider, extend_rejects_duplicate_digest_algorithms_before_dispatch)
{
    // Verifies PCR extend rejects duplicate bank digests without sending a TPM command.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::array<tpmkit::pcr::digest_value, 2U> digests{
        {sha256_digest(0x01U), sha256_digest(0x02U)}};

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(digests));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(fake.transmitted_commands().empty());
}

TEST(esys_pcr_provider, extend_does_not_call_observer_on_failure)
{
    // Verifies PCR extend suppresses observer callbacks when ESYS fails.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_LOCALITY));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider_owner = require_pcr_provider(context.value(), &observer);
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const tpmkit::pcr::digest_value digest = sha256_digest(0x40U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 0U);
}

TEST(esys_pcr_provider, event_rejects_missing_esys_context)
{
    // Verifies PCR event requires an ESYS context.

    tpmkit::detail::esys::esys_pcr_provider provider{nullptr, default_log(), nullptr};
    const std::vector<std::uint8_t> event_data{0x01U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, event_rejects_oversized_event_data)
{
    // Verifies PCR event rejects payloads beyond TPM2B_EVENT capacity.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data(sizeof(TPM2B_EVENT::buffer) + 1U, 0xabU);

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(fake.transmits_observed(), 0U);
}

TEST(esys_pcr_provider, event_returns_error_without_observer_on_failure)
{
    // Verifies PCR event failures do not notify observers.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_LOCALITY));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider_owner = require_pcr_provider(context.value(), &observer);
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data{0x02U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(observer.event_calls, 0U);
}

TEST(esys_pcr_provider, event_rejects_unknown_digest_algorithm_from_tpm)
{
    // Verifies PCR event rejects result digests from unsupported TPM banks.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(
        event_success_response_with_algorithm(TPM2_ALG_SHA3_256, digest_bytes(0x81U)));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data{0x03U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, extend_succeeds_with_null_observer)
{
    // Verifies PCR extend has no observer dependency when observer is null.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const tpmkit::pcr::digest_value digest = sha256_digest(0x50U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    EXPECT_TRUE(result.has_value());
}

TEST(esys_pcr_provider, extend_emits_success_log_fields)
{
    // Verifies PCR extend success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(extend_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const tpmkit::pcr::digest_value digest = sha256_digest(0x60U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_extend_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(field_value(records.front(), events::fields::bank_count), "1");
}

TEST(esys_pcr_provider, event_passes_event_data_and_returns_result)
{
    // Verifies PCR event sends event bytes and returns TPM-computed digests.

    tpmkit::testing::fake_tcti fake;
    const auto digest = digest_bytes(0x70U);
    fake.push_response(event_success_response(digest));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data{0xa0U, 0xa1U, 0xa2U, 0xa3U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().digests.size(), 1U);
    EXPECT_EQ(result.value().digests.front().digest(), digest);
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Event);
    EXPECT_TRUE(contains_bytes(commands.front(), event_data));
}

TEST(esys_pcr_provider, event_calls_observer_with_result)
{
    // Verifies PCR event observer receives the raw event data and returned digests.

    tpmkit::testing::fake_tcti fake;
    const auto digest = digest_bytes(0x80U);
    fake.push_response(event_success_response(digest));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    recording_observer observer;
    auto provider_owner = require_pcr_provider(context.value(), &observer);
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data{0xb0U, 0xb1U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.event_calls, 1U);
    EXPECT_EQ(observer.event_index, 16U);
    EXPECT_EQ(observer.event_bytes, event_data);
    EXPECT_EQ(observer.event_result, result.value());
}

TEST(esys_pcr_provider, event_emits_success_log_fields)
{
    // Verifies PCR event success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(event_success_response(digest_bytes(0x90U)));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::vector<std::uint8_t> event_data{0xc0U, 0xc1U, 0xc2U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_event_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(field_value(records.front(), events::fields::event_size), "3");
}

TEST(esys_pcr_provider, failure_emits_pcr_tss_error_without_measurement_bytes)
{
    // Verifies PCR failures log TSS metadata without digest or event payload values.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_LOCALITY));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const tpmkit::pcr::digest_value digest = sha256_digest(0xd0U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_tss_error);
    EXPECT_EQ(field_value(records.front(), events::fields::operation), "pcr_extend");
    EXPECT_EQ(field_value(records.front(), events::fields::tss_rc_hex), "0x00000907");
    EXPECT_EQ(field_value(records.front(), events::fields::tss_layer), "tpm");
    EXPECT_FALSE(contains_field_value(records, "d0"));
}

TEST(esys_pcr_provider, reset_succeeds_for_resettable_pcr)
{
    // Verifies PCR reset sends the reset command for a resettable PCR.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(simple_session_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto reset = provider.reset(tpmkit::pcr::index::debug);

    ASSERT_TRUE(reset.has_value());
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Reset);
}

TEST(esys_pcr_provider, reset_returns_resource_error_for_non_resettable_pcr)
{
    // Verifies TPM locality rejection for non-resettable PCRs is translated.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_LOCALITY));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto reset = provider.reset(tpmkit::pcr::index::firmware_0);

    ASSERT_FALSE(reset.has_value());
    EXPECT_EQ(reset.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, reset_emits_success_log_fields)
{
    // Verifies PCR reset success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(simple_session_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto reset = provider.reset(tpmkit::pcr::index::debug);

    ASSERT_TRUE(reset.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_reset_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
}

TEST(esys_pcr_provider, set_auth_policy_uses_platform_authorization)
{
    // Verifies PCR auth policy uses platform authorization and sends the policy digest.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(simple_session_success_response());
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const auto policy_digest = digest_bytes(0xe0U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_TRUE(result.has_value());
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_SetAuthPolicy);
    EXPECT_EQ(read_u32(commands.front(), 10U), TPM2_RH_PLATFORM);
    EXPECT_TRUE(contains_bytes(commands.front(), policy_digest));
}

TEST(esys_pcr_provider, set_auth_policy_returns_error_when_platform_auth_unavailable)
{
    // Verifies platform authorization failures surface as security failures.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_AUTH_FAIL));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const auto policy_digest = digest_bytes(0xe4U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::security_failure);
}

TEST(esys_pcr_provider, set_auth_policy_rejects_invalid_algorithm_before_dispatch)
{
    // Verifies unsupported policy algorithms return input_error without throwing or dispatching.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::array<std::uint8_t, 1U> policy_digest{{0x01U}};

    const auto result =
        provider.set_auth_policy(tpmkit::pcr::index::debug, static_cast<tpmkit::hash_algorithm>(99),
                                 gsl::make_span(policy_digest));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(fake.transmitted_commands().empty());
}

TEST(esys_pcr_provider, set_auth_policy_emits_success_log_fields)
{
    // Verifies PCR auth policy success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(simple_session_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const auto policy_digest = digest_bytes(0xe8U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_auth_policy_set);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(field_value(records.front(), events::fields::policy_algorithm), "sha256");
}

TEST(esys_pcr_provider, set_auth_value_rejects_non_empty_auth_without_secure_transport)
{
    // Verifies non-empty auth values fail closed until protected transport is available.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    tpmkit::secret_buffer auth{std::vector<std::uint8_t>{0x73U, 0x65U, 0x6bU, 0x72U, 0x69U, 0x74U}};

    const auto result = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    const auto commands = fake.transmitted_commands();
    EXPECT_TRUE(commands.empty());
}

TEST(esys_pcr_provider, set_auth_value_does_not_dispatch_non_empty_auth_material)
{
    // Verifies a non-empty auth value is not sent under an unprotected session.

    tpmkit::testing::fake_tcti fake;
    const std::vector<std::uint8_t> secret{0x73U, 0x65U, 0x6bU, 0x72U, 0x69U, 0x74U};
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::vector<std::uint8_t> owned_secret = secret;
    tpmkit::secret_buffer auth{std::move(owned_secret)};

    const auto result = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    const auto commands = fake.transmitted_commands();
    EXPECT_TRUE(commands.empty());
}

TEST(esys_pcr_provider, set_auth_value_emits_success_log_without_auth_value)
{
    // Verifies empty SetAuthValue success logs no authorization value bytes.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(simple_session_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    tpmkit::secret_buffer auth;

    const auto result = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));

    ASSERT_TRUE(result.has_value());
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_SetAuthValue);
    EXPECT_EQ(read_u16(commands.front(), command_parameter_offset(commands.front())), 0U);
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_auth_value_set);
    EXPECT_EQ(field_value(records.front(), events::fields::pcr_index), "16");
}

TEST(esys_pcr_provider, auth_operation_logs_do_not_leak_secret_material)
{
    // Verifies PCR auth operation logs omit auth values and policy digest material.

    tpmkit::testing::fake_tcti fake;
    const std::vector<std::uint8_t> secret{0x73U, 0x65U, 0x6bU, 0x72U, 0x69U, 0x74U};
    fake.push_response(simple_session_success_response());
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::vector<std::uint8_t> owned_secret = secret;
    tpmkit::secret_buffer auth{std::move(owned_secret)};
    const auto policy_digest = digest_bytes(0xdeU);

    const auto auth_value = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));
    const auto auth_policy = provider.set_auth_policy(
        tpmkit::pcr::index::debug, tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_FALSE(auth_value.has_value());
    ASSERT_TRUE(auth_policy.has_value());
    const auto records = log->snapshot();
    EXPECT_FALSE(contains_field_value(records, "sekrit"));
    EXPECT_FALSE(contains_field_value(records, "de"));
}

TEST(esys_pcr_provider, allocate_translates_bank_list_to_full_pcr_selections)
{
    // Verifies PCR allocate sends one full-PCR selection per requested bank.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(allocate_success_response(true, 32U, 12U, 44U));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 2U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256},
                                             tpmkit::pcr::bank{tpmkit::hash_algorithm::sha384}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    const auto commands = fake.transmitted_commands();
    ASSERT_EQ(commands.size(), 1U);
    EXPECT_EQ(read_u32(commands.front(), 6U), TPM2_CC_PCR_Allocate);
    EXPECT_EQ(read_u32(commands.front(), 10U), TPM2_RH_PLATFORM);
    EXPECT_EQ(allocation_selection_count(commands.front()), 2U);
    EXPECT_TRUE(has_standard_pcr_bank_selection(commands.front(), TPM2_ALG_SHA256));
    EXPECT_TRUE(has_standard_pcr_bank_selection(commands.front(), TPM2_ALG_SHA384));
}

TEST(esys_pcr_provider, allocate_returns_result_fields_on_success)
{
    // Verifies PCR allocate returns every result field reported by the TPM.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(allocate_success_response(true, 32U, 88U, 120U));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    EXPECT_TRUE(allocate.value().allocation_success);
    EXPECT_EQ(allocate.value().max_pcr, 32U);
    EXPECT_EQ(allocate.value().size_needed, 88U);
    EXPECT_EQ(allocate.value().size_available, 120U);
}

TEST(esys_pcr_provider, allocate_rejects_duplicate_banks_before_dispatch)
{
    // Verifies PCR allocate rejects duplicate banks without sending a TPM command.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    const std::array<tpmkit::pcr::bank, 2U> banks{
        {tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256},
         tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(fake.transmitted_commands().empty());
}

TEST(esys_pcr_provider, allocate_rejects_empty_bank_list_before_dispatch)
{
    // Verifies PCR allocate requires at least one bank and sends no TPM command on empty input.

    tpmkit::testing::fake_tcti fake;
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;

    const auto allocate = provider.allocate(gsl::span<const tpmkit::pcr::bank>{});

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(fake.transmitted_commands().empty());
}

TEST(esys_pcr_provider, allocate_returns_security_failure_when_platform_auth_unavailable)
{
    // Verifies platform authorization failures are translated for PCR allocate.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_AUTH_FAIL));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::security_failure);
}

TEST(esys_pcr_provider, allocate_handles_partial_allocation_result)
{
    // Verifies PCR allocate preserves a TPM-reported partial allocation result.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(allocate_success_response(false, 32U, 90U, 11U));
    auto context = tpmkit::tpm_context::create(owned_config(fake));
    ASSERT_TRUE(context.has_value());
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha512}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    EXPECT_FALSE(allocate.value().allocation_success);
    EXPECT_EQ(allocate.value().max_pcr, 32U);
    EXPECT_EQ(allocate.value().size_needed, 90U);
    EXPECT_EQ(allocate.value().size_available, 11U);
}

TEST(esys_pcr_provider, allocate_emits_success_log_fields)
{
    // Verifies PCR allocate success emits the documented PCR log event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(allocate_success_response(true, 32U, 12U, 44U));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha384}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_allocate_completed);
    EXPECT_EQ(field_value(records.front(), events::fields::bank_count), "1");
    EXPECT_EQ(field_value(records.front(), events::fields::allocation_success), "true");
}

TEST(esys_pcr_provider, allocate_emits_pcr_tss_error_on_failure)
{
    // Verifies PCR allocate failures emit the PCR-specific TSS error event.

    tpmkit::testing::fake_tcti fake;
    fake.push_response(error_response(TPM2_RC_AUTH_FAIL));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto context = tpmkit::tpm_context::create(owned_config(fake, log));
    ASSERT_TRUE(context.has_value());
    log->clear();
    auto provider_owner = require_pcr_provider(context.value());
    ASSERT_NE(provider_owner, nullptr);
    tpmkit::pcr::provider& provider = *provider_owner;
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(std::string_view{records.front().message}, events::pcr_tss_error);
    EXPECT_EQ(field_value(records.front(), events::fields::operation), "pcr_allocate");
    EXPECT_EQ(field_value(records.front(), events::fields::tss_layer), "tpm");
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace
