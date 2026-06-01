#include "esys_fake_api.h"

#include <tpmkit/encoding/hex.h>
#include <tpmkit/logging/noop_logger.h>

#include "src/adapters/tpm2_esys/pcr/esys_pcr_provider.h"
#include "src/adapters/tpm2_esys/support/log_events.h"

#include <gsl/span>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace events = tpmkit::detail::tpm2_esys::events;
namespace fake = tpmkit::testing::esys;

tpmkit::logger& default_log()
{
    return tpmkit::noop_logger::instance();
}

class recording_observer final : public tpmkit::pcr::observer {
public:
    void on_event(const tpmkit::pcr::index index, const gsl::span<const std::uint8_t> event_data,
                  const tpmkit::pcr::event_result& result) noexcept final
    {
        event_index = index.value();
        event_bytes.assign(event_data.begin(), event_data.end());
        event_result = result;
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
    std::uint8_t event_index{0U};
    std::uint8_t extended_index{0U};
    std::vector<std::uint8_t> event_bytes;
    std::vector<tpmkit::pcr::digest_value> extended_digests;
    tpmkit::pcr::event_result event_result;
};

tpmkit::detail::tpm2_esys::esys_pcr_provider
make_provider(fake::fake_esys_state& state, tpmkit::logger& log,
              tpmkit::pcr::observer* const observer = nullptr)
{
    return tpmkit::detail::tpm2_esys::esys_pcr_provider{fake::esys_handle(state), log, observer,
                                                        fake::fake_api()};
}

tpmkit::detail::tpm2_esys::esys_pcr_provider
make_provider(fake::fake_esys_state& state, tpmkit::pcr::observer* const observer = nullptr)
{
    return make_provider(state, default_log(), observer);
}

std::string uppercase(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }

    return value;
}

bool contains_any_pattern(const std::string& text, const std::vector<std::string>& patterns)
{
    return std::any_of(patterns.begin(), patterns.end(), [&text](const std::string& pattern) {
        return !pattern.empty() && text.find(pattern) != std::string::npos;
    });
}

bool records_contain_any_pattern(const std::vector<tpmkit::testing::log_record>& records,
                                 const std::vector<std::string>& patterns)
{
    for (const auto& record : records) {
        if (contains_any_pattern(record.message, patterns)) {
            return true;
        }
        for (const auto& field : record.fields) {
            if (contains_any_pattern(field.first, patterns) ||
                contains_any_pattern(field.second, patterns)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> secret_patterns(const std::vector<std::uint8_t>& bytes)
{
    const std::string raw{bytes.begin(), bytes.end()};
    const std::string hex = tpmkit::encoding::encode_hex(bytes);
    return {raw, hex, uppercase(hex)};
}

void append_patterns(std::vector<std::string>& target, const std::vector<std::uint8_t>& bytes)
{
    auto patterns = secret_patterns(bytes);
    target.insert(target.end(), patterns.begin(), patterns.end());
}

void expect_success_log_schema(const tpmkit::testing::log_record& record,
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

void expect_error_log_schema(const tpmkit::testing::log_record& record,
                             const events::event_descriptor& descriptor,
                             const std::string_view category, const std::string_view operation,
                             const std::string_view error_code, const std::string_view layer)
{
    EXPECT_EQ(record.level, tpmkit::log_level::error);
    EXPECT_EQ(record.message, std::string{descriptor.message});
    EXPECT_EQ(fake::field_value(record, events::fields::event), std::string{descriptor.name});
    EXPECT_EQ(fake::field_value(record, events::fields::component),
              std::string{events::component_tpm2_esys});
    EXPECT_EQ(fake::field_value(record, events::fields::outcome),
              std::string{events::values::failure});
    EXPECT_EQ(fake::field_value(record, events::fields::error_category), std::string{category});
    EXPECT_EQ(fake::field_value(record, events::fields::operation), std::string{operation});
    EXPECT_EQ(fake::field_value(record, events::fields::error_code), std::string{error_code});
    EXPECT_EQ(fake::field_value(record, events::fields::tss_layer), std::string{layer});
    EXPECT_EQ(fake::field_value(record, events::fields::backend_error_description),
              "fake esys error");
}

void expect_password_sessions(const ESYS_TR shandle1, const ESYS_TR shandle2,
                              const ESYS_TR shandle3)
{
    EXPECT_EQ(shandle1, ESYS_TR_PASSWORD);
    EXPECT_EQ(shandle2, ESYS_TR_NONE);
    EXPECT_EQ(shandle3, ESYS_TR_NONE);
}

void expect_no_sessions(const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3)
{
    EXPECT_EQ(shandle1, ESYS_TR_NONE);
    EXPECT_EQ(shandle2, ESYS_TR_NONE);
    EXPECT_EQ(shandle3, ESYS_TR_NONE);
}

std::array<tpmkit::pcr::digest_value, 2U> duplicate_sha256_digests()
{
    return {fake::sha256_digest(0x01U), fake::sha256_digest(0x02U)};
}

TPML_PCR_SELECTION two_pcr_sha256_selection()
{
    TPML_PCR_SELECTION selection{};
    selection.count = 1U;
    selection.pcrSelections[0U].hash = TPM2_ALG_SHA256;
    selection.pcrSelections[0U].sizeofSelect = TPM2_PCR_SELECT_MAX;
    selection.pcrSelections[0U].pcrSelect[2U] = 0x03U;
    return selection;
}

} // namespace

TEST(esys_pcr_provider, read_translates_selection_and_returns_result)
{
    // Verifies PCR read translates selection fields and returns domain values.

    fake::fake_esys_state state;
    const auto digest = fake::digest_bytes(0x10U);
    state.read_update_counter = 7U;
    state.read_actual_selection = fake::sha256_selection(tpmkit::pcr::index::debug.value());
    state.read_values = fake::read_values(digest);
    auto provider = make_provider(state);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->update_counter, 7U);
    ASSERT_EQ(result->values.size(), 1U);
    EXPECT_EQ(result->values.front().index, tpmkit::pcr::index::debug);
    EXPECT_EQ(result->values.front().digest.digest(), digest);
    ASSERT_EQ(state.read_calls.size(), 1U);
    const auto& call = state.read_calls.front();
    expect_no_sessions(call.shandle1, call.shandle2, call.shandle3);
    ASSERT_EQ(call.selection.count, 1U);
    EXPECT_EQ(call.selection.pcrSelections[0U].hash, TPM2_ALG_SHA256);
    EXPECT_EQ(call.selection.pcrSelections[0U].sizeofSelect, 3U);
    EXPECT_EQ(call.selection.pcrSelections[0U].pcrSelect[2U], 0x01U);
}

TEST(esys_pcr_provider, read_uses_four_byte_selection_for_high_pcr_index)
{
    // Verifies PCR indices above 23 are represented by a four-byte selection.

    fake::fake_esys_state state;
    state.read_actual_selection.count = 0U;
    state.read_values.count = 0U;
    auto provider = make_provider(state);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index{31U}}});

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(state.read_calls.size(), 1U);
    const auto& selection = state.read_calls.front().selection.pcrSelections[0U];
    EXPECT_EQ(selection.hash, TPM2_ALG_SHA256);
    EXPECT_EQ(selection.sizeofSelect, 4U);
    EXPECT_EQ(selection.pcrSelect[3U], 0x80U);
}

TEST(esys_pcr_provider, read_returns_resource_error_for_transport_failure)
{
    // Verifies PCR read maps transport failures to resource errors.

    fake::fake_esys_state state;
    state.pcr_read_rc = TSS2_TCTI_RC_IO_ERROR;
    auto provider = make_provider(state);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, read_returns_backend_error_for_unexpected_tpm_code)
{
    // Verifies unknown TPM failures remain backend errors.

    fake::fake_esys_state state;
    state.pcr_read_rc = TPM2_RC_INITIALIZE;
    auto provider = make_provider(state);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_rejects_digest_count_mismatch_from_tpm)
{
    // Verifies PCR read rejects TPM responses whose digest count does not match selection.

    fake::fake_esys_state state;
    state.read_actual_selection = two_pcr_sha256_selection();
    state.read_values = fake::read_values(fake::digest_bytes(0x12U));
    auto provider = make_provider(state);

    const auto result = provider.read(tpmkit::pcr::selection{
        tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug, tpmkit::pcr::index::drtm_17}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_emits_success_log_fields)
{
    // Verifies PCR read success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    state.read_actual_selection = fake::sha256_selection(tpmkit::pcr::index::debug.value());
    state.read_values = fake::read_values(fake::digest_bytes(0x20U));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_read_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::bank), "sha256");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_count), "1");
}

TEST(esys_pcr_provider, read_empty_requested_selection_returns_empty_result)
{
    // Verifies PCR read maps an empty requested selection to an empty result.

    fake::fake_esys_state state;
    state.read_actual_selection.count = 0U;
    state.read_values.count = 0U;
    auto provider = make_provider(state);

    const auto result = provider.read(tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->actual_selection, tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256});
    EXPECT_TRUE(result->values.empty());
}

TEST(esys_pcr_provider, read_rejects_bad_digest_size_from_tpm)
{
    // Verifies malformed PCR read digest sizes return backend errors.

    fake::fake_esys_state state;
    state.read_actual_selection = fake::sha256_selection(tpmkit::pcr::index::debug.value());
    state.read_values.count = 1U;
    state.read_values.digests[0U].size = 31U;
    auto provider = make_provider(state);

    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, read_rejects_missing_esys_context)
{
    // Verifies PCR read requires an ESYS context.

    tpmkit::detail::tpm2_esys::esys_pcr_provider provider{nullptr, default_log(), nullptr,
                                                          fake::fake_api()};

    const auto result = provider.read(tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, extend_translates_digest_and_calls_observer)
{
    // Verifies PCR extend sends digest values and notifies the observer after success.

    fake::fake_esys_state state;
    recording_observer observer;
    auto provider = make_provider(state, &observer);
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x30U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 1U);
    EXPECT_EQ(observer.extended_index, 16U);
    ASSERT_EQ(observer.extended_digests.size(), 1U);
    EXPECT_EQ(observer.extended_digests.front(), digest);
    ASSERT_EQ(state.extend_calls.size(), 1U);
    const auto& call = state.extend_calls.front();
    EXPECT_EQ(call.pcr_handle, tpmkit::detail::tpm2_esys::pcr_handle(tpmkit::pcr::index::debug));
    expect_password_sessions(call.shandle1, call.shandle2, call.shandle3);
    ASSERT_EQ(call.digests.count, 1U);
    EXPECT_EQ(call.digests.digests[0U].hashAlg, TPM2_ALG_SHA256);
    EXPECT_EQ(
        std::vector<std::uint8_t>(call.digests.digests[0U].digest.sha256,
                                  call.digests.digests[0U].digest.sha256 + TPM2_SHA256_DIGEST_SIZE),
        digest.digest());
}

TEST(esys_pcr_provider, extend_rejects_missing_esys_context)
{
    // Verifies PCR extend requires an ESYS context.

    tpmkit::detail::tpm2_esys::esys_pcr_provider provider{nullptr, default_log(), nullptr,
                                                          fake::fake_api()};
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x38U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, extend_rejects_too_many_digests)
{
    // Verifies PCR extend validates the TPM digest-list capacity before dispatch.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    std::vector<tpmkit::pcr::digest_value> digests;
    digests.reserve(TPM2_NUM_PCR_BANKS + 1U);
    for (std::size_t index = 0U; index < TPM2_NUM_PCR_BANKS + 1U; ++index) {
        digests.push_back(fake::sha256_digest(static_cast<std::uint8_t>(0x39U + index)));
    }

    const auto result = provider.extend(tpmkit::pcr::index::debug, digests);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.extend_calls.empty());
}

TEST(esys_pcr_provider, extend_rejects_empty_digest_list_before_dispatch)
{
    // Verifies PCR extend requires at least one digest and sends no TPM command on empty input.

    fake::fake_esys_state state;
    auto provider = make_provider(state);

    const auto result =
        provider.extend(tpmkit::pcr::index::debug, gsl::span<const tpmkit::pcr::digest_value>{});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.extend_calls.empty());
}

TEST(esys_pcr_provider, extend_rejects_duplicate_digest_algorithms_before_dispatch)
{
    // Verifies PCR extend rejects duplicate bank digests without sending a TPM command.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const auto digests = duplicate_sha256_digests();

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(digests));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.extend_calls.empty());
}

TEST(esys_pcr_provider, extend_does_not_call_observer_on_failure)
{
    // Verifies PCR extend suppresses observer callbacks when ESYS fails.

    fake::fake_esys_state state;
    state.pcr_extend_rc = TPM2_RC_LOCALITY;
    recording_observer observer;
    auto provider = make_provider(state, &observer);
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x40U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(observer.extend_calls, 0U);
}

TEST(esys_pcr_provider, extend_succeeds_with_null_observer)
{
    // Verifies PCR extend has no observer dependency when observer is null.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x50U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    EXPECT_TRUE(result.has_value());
}

TEST(esys_pcr_provider, extend_emits_success_log_fields)
{
    // Verifies PCR extend success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0x60U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_extend_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::bank_count), "1");
}

TEST(esys_pcr_provider, event_rejects_missing_esys_context)
{
    // Verifies PCR event requires an ESYS context.

    tpmkit::detail::tpm2_esys::esys_pcr_provider provider{nullptr, default_log(), nullptr,
                                                          fake::fake_api()};
    const std::vector<std::uint8_t> event_data{0x01U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, event_rejects_oversized_event_data)
{
    // Verifies PCR event rejects payloads beyond TPM2B_EVENT capacity before dispatch.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const std::vector<std::uint8_t> event_data(sizeof(TPM2B_EVENT::buffer) + 1U, 0xabU);

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.event_calls.empty());
}

TEST(esys_pcr_provider, event_returns_error_without_observer_on_failure)
{
    // Verifies PCR event failures do not notify observers.

    fake::fake_esys_state state;
    state.pcr_event_rc = TPM2_RC_LOCALITY;
    recording_observer observer;
    auto provider = make_provider(state, &observer);
    const std::vector<std::uint8_t> event_data{0x02U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(observer.event_calls, 0U);
}

TEST(esys_pcr_provider, event_rejects_unknown_digest_algorithm_from_tpm)
{
    // Verifies PCR event rejects result digests from unsupported TPM banks.

    fake::fake_esys_state state;
    state.event_digests.count = 1U;
    state.event_digests.digests[0U].hashAlg = TPM2_ALG_SHA3_256;
    auto provider = make_provider(state);
    const std::vector<std::uint8_t> event_data{0x03U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::backend_error);
}

TEST(esys_pcr_provider, event_passes_event_data_and_returns_result)
{
    // Verifies PCR event sends event bytes and returns TPM-computed digests.

    fake::fake_esys_state state;
    const auto digest = fake::sha256_digest(0x70U);
    state.event_digests = fake::digest_values(digest);
    auto provider = make_provider(state);
    const std::vector<std::uint8_t> event_data{0xa0U, 0xa1U, 0xa2U, 0xa3U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->digests.size(), 1U);
    EXPECT_EQ(result->digests.front(), digest);
    ASSERT_EQ(state.event_calls.size(), 1U);
    const auto& call = state.event_calls.front();
    EXPECT_EQ(call.pcr_handle, tpmkit::detail::tpm2_esys::pcr_handle(tpmkit::pcr::index::debug));
    expect_password_sessions(call.shandle1, call.shandle2, call.shandle3);
    EXPECT_EQ(call.event_data.size, event_data.size());
    EXPECT_EQ(std::vector<std::uint8_t>(call.event_data.buffer,
                                        call.event_data.buffer + call.event_data.size),
              event_data);
}

TEST(esys_pcr_provider, event_calls_observer_with_result)
{
    // Verifies PCR event observer receives the raw event data and returned digests.

    fake::fake_esys_state state;
    state.event_digests = fake::digest_values(fake::sha256_digest(0x80U));
    recording_observer observer;
    auto provider = make_provider(state, &observer);
    const std::vector<std::uint8_t> event_data{0xb0U, 0xb1U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(observer.event_calls, 1U);
    EXPECT_EQ(observer.event_index, 16U);
    EXPECT_EQ(observer.event_bytes, event_data);
    EXPECT_EQ(observer.event_result, *result);
}

TEST(esys_pcr_provider, event_emits_success_log_fields)
{
    // Verifies PCR event success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    state.event_digests = fake::digest_values(fake::sha256_digest(0x90U));
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    const std::vector<std::uint8_t> event_data{0xc0U, 0xc1U, 0xc2U};

    const auto result = provider.event(tpmkit::pcr::index::debug, event_data);

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_event_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::event_size), "3");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::bank_count), "1");
}

TEST(esys_pcr_provider, failure_emits_pcr_tss_error_without_measurement_bytes)
{
    // Verifies PCR failures log TSS metadata without digest or event payload values.

    fake::fake_esys_state state;
    state.pcr_extend_rc = TPM2_RC_LOCALITY;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    const tpmkit::pcr::digest_value digest = fake::sha256_digest(0xd0U);

    const auto result = provider.extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    ASSERT_FALSE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_error_log_schema(records.front(), events::pcr_tss_error, events::values::resource_error,
                            "pcr_extend", "0x00000907", "tpm");
    EXPECT_FALSE(
        fake::contains_field_value(records, tpmkit::encoding::encode_hex(digest.digest())));
    EXPECT_FALSE(fake::contains_field_value(
        records, uppercase(tpmkit::encoding::encode_hex(digest.digest()))));
}

TEST(esys_pcr_provider, reset_succeeds_for_resettable_pcr)
{
    // Verifies PCR reset sends the reset command for a resettable PCR.

    fake::fake_esys_state state;
    auto provider = make_provider(state);

    const auto reset = provider.reset(tpmkit::pcr::index::debug);

    ASSERT_TRUE(reset.has_value());
    ASSERT_EQ(state.reset_calls.size(), 1U);
    const auto& call = state.reset_calls.front();
    EXPECT_EQ(call.pcr_handle, tpmkit::detail::tpm2_esys::pcr_handle(tpmkit::pcr::index::debug));
    expect_password_sessions(call.shandle1, call.shandle2, call.shandle3);
}

TEST(esys_pcr_provider, reset_returns_resource_error_for_non_resettable_pcr)
{
    // Verifies TPM locality rejection for non-resettable PCRs is translated.

    fake::fake_esys_state state;
    state.pcr_reset_rc = TPM2_RC_LOCALITY;
    auto provider = make_provider(state);

    const auto reset = provider.reset(tpmkit::pcr::index::firmware_0);

    ASSERT_FALSE(reset.has_value());
    EXPECT_EQ(reset.error().category, tpmkit::error_category::resource_error);
}

TEST(esys_pcr_provider, reset_emits_success_log_fields)
{
    // Verifies PCR reset success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);

    const auto reset = provider.reset(tpmkit::pcr::index::debug);

    ASSERT_TRUE(reset.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_reset_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
}

TEST(esys_pcr_provider, set_auth_policy_uses_platform_authorization)
{
    // Verifies PCR auth policy uses platform authorization and sends the policy digest.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const auto policy_digest = fake::digest_bytes(0xe0U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(state.set_auth_policy_calls.size(), 1U);
    const auto& call = state.set_auth_policy_calls.front();
    EXPECT_EQ(call.auth_handle, ESYS_TR_RH_PLATFORM);
    expect_password_sessions(call.shandle1, call.shandle2, call.shandle3);
    EXPECT_EQ(call.hash_alg, TPM2_ALG_SHA256);
    EXPECT_EQ(call.pcr_num, TPM2_HR_PCR + tpmkit::pcr::index::debug.value());
    EXPECT_EQ(call.auth_policy.size, policy_digest.size());
    EXPECT_EQ(std::vector<std::uint8_t>(call.auth_policy.buffer,
                                        call.auth_policy.buffer + call.auth_policy.size),
              policy_digest);
}

TEST(esys_pcr_provider, set_auth_policy_returns_error_when_platform_auth_unavailable)
{
    // Verifies platform authorization failures surface as security failures.

    fake::fake_esys_state state;
    state.pcr_set_auth_policy_rc = TPM2_RC_AUTH_FAIL;
    auto provider = make_provider(state);
    const auto policy_digest = fake::digest_bytes(0xe4U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::security_failure);
}

TEST(esys_pcr_provider, set_auth_policy_rejects_invalid_algorithm_before_dispatch)
{
    // Verifies unsupported policy algorithms return input_error without dispatching.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const std::array<std::uint8_t, 1U> policy_digest{{0x01U}};

    const auto result =
        provider.set_auth_policy(tpmkit::pcr::index::debug, static_cast<tpmkit::hash_algorithm>(99),
                                 gsl::make_span(policy_digest));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.set_auth_policy_calls.empty());
}

TEST(esys_pcr_provider, set_auth_policy_rejects_wrong_digest_size_before_dispatch)
{
    // Verifies PCR auth policy checks digest size before calling ESYS.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const std::array<std::uint8_t, 1U> policy_digest{{0x01U}};

    const auto result = provider.set_auth_policy(
        tpmkit::pcr::index::debug, tpmkit::hash_algorithm::sha256, gsl::make_span(policy_digest));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.set_auth_policy_calls.empty());
}

TEST(esys_pcr_provider, set_auth_policy_emits_success_log_fields)
{
    // Verifies PCR auth policy success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    const auto policy_digest = fake::digest_bytes(0xe8U);

    const auto result = provider.set_auth_policy(tpmkit::pcr::index::debug,
                                                 tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_TRUE(result.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_auth_policy_set);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::policy_algorithm), "sha256");
}

TEST(esys_pcr_provider, set_auth_value_rejects_non_empty_auth_without_secure_transport)
{
    // Verifies non-empty auth values fail closed before dispatch.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    tpmkit::secret_buffer auth{std::vector<std::uint8_t>{0x73U, 0x65U, 0x6bU, 0x72U, 0x69U, 0x74U}};

    const auto result = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().category, tpmkit::error_category::resource_error);
    EXPECT_TRUE(state.tr_set_auth_calls.empty());
    EXPECT_TRUE(state.set_auth_value_calls.empty());
}

TEST(esys_pcr_provider, set_auth_value_emits_success_log_without_auth_value)
{
    // Verifies empty SetAuthValue sends empty auth and logs no authorization bytes.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    tpmkit::secret_buffer auth;

    const auto result = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(state.tr_set_auth_calls.size(), 2U);
    EXPECT_EQ(state.tr_set_auth_calls[0U].handle,
              tpmkit::detail::tpm2_esys::pcr_handle(tpmkit::pcr::index::debug));
    EXPECT_EQ(state.tr_set_auth_calls[0U].auth.size, 0U);
    ASSERT_EQ(state.set_auth_value_calls.size(), 1U);
    EXPECT_EQ(state.set_auth_value_calls.front().auth.size, 0U);
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_auth_value_set);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::pcr_index), "16");
}

TEST(esys_pcr_provider, auth_operation_logs_do_not_leak_secret_material)
{
    // Verifies PCR auth operation logs omit raw and encoded auth or policy bytes.

    fake::fake_esys_state state;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    const std::vector<std::uint8_t> secret{'a', 'u', 't', 'h', '-', 'c', 'a', 'n', 'a', 'r', 'y'};
    std::vector<std::uint8_t> owned_secret = secret;
    tpmkit::secret_buffer auth{std::move(owned_secret)};
    const auto policy_digest = fake::digest_bytes(0xdeU);

    const auto auth_value = provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));
    const auto auth_policy = provider.set_auth_policy(
        tpmkit::pcr::index::debug, tpmkit::hash_algorithm::sha256, policy_digest);

    ASSERT_FALSE(auth_value.has_value());
    ASSERT_TRUE(auth_policy.has_value());
    std::vector<std::string> patterns;
    append_patterns(patterns, secret);
    append_patterns(patterns, policy_digest);
    EXPECT_FALSE(records_contain_any_pattern(log->snapshot(), patterns));
}

TEST(esys_pcr_provider, allocate_translates_bank_list_to_full_pcr_selections)
{
    // Verifies PCR allocate sends one full-PCR selection per requested bank.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    std::array<tpmkit::pcr::bank, 2U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256},
                                             tpmkit::pcr::bank{tpmkit::hash_algorithm::sha384}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    ASSERT_EQ(state.allocate_calls.size(), 1U);
    const auto& call = state.allocate_calls.front();
    EXPECT_EQ(call.auth_handle, ESYS_TR_RH_PLATFORM);
    expect_password_sessions(call.shandle1, call.shandle2, call.shandle3);
    ASSERT_EQ(call.allocation.count, 2U);
    EXPECT_EQ(call.allocation.pcrSelections[0U].hash, TPM2_ALG_SHA256);
    EXPECT_EQ(call.allocation.pcrSelections[1U].hash, TPM2_ALG_SHA384);
    for (UINT32 selection_index = 0U; selection_index < call.allocation.count; ++selection_index) {
        EXPECT_EQ(call.allocation.pcrSelections[selection_index].sizeofSelect, 3U);
        EXPECT_EQ(call.allocation.pcrSelections[selection_index].pcrSelect[0U], 0xffU);
        EXPECT_EQ(call.allocation.pcrSelections[selection_index].pcrSelect[1U], 0xffU);
        EXPECT_EQ(call.allocation.pcrSelections[selection_index].pcrSelect[2U], 0xffU);
    }
}

TEST(esys_pcr_provider, allocate_returns_result_fields_on_success)
{
    // Verifies PCR allocate returns every result field reported by the TPM.

    fake::fake_esys_state state;
    state.allocation_success = TPM2_YES;
    state.max_pcr = 32U;
    state.size_needed = 88U;
    state.size_available = 120U;
    auto provider = make_provider(state);
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    EXPECT_TRUE(allocate->allocation_success);
    EXPECT_EQ(allocate->max_pcr, 32U);
    EXPECT_EQ(allocate->size_needed, 88U);
    EXPECT_EQ(allocate->size_available, 120U);
}

TEST(esys_pcr_provider, allocate_rejects_duplicate_banks_before_dispatch)
{
    // Verifies PCR allocate rejects duplicate banks without sending a TPM command.

    fake::fake_esys_state state;
    auto provider = make_provider(state);
    const std::array<tpmkit::pcr::bank, 2U> banks{
        {tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256},
         tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.allocate_calls.empty());
}

TEST(esys_pcr_provider, allocate_rejects_empty_bank_list_before_dispatch)
{
    // Verifies PCR allocate requires at least one bank and sends no TPM command on empty input.

    fake::fake_esys_state state;
    auto provider = make_provider(state);

    const auto allocate = provider.allocate(gsl::span<const tpmkit::pcr::bank>{});

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::input_error);
    EXPECT_TRUE(state.allocate_calls.empty());
}

TEST(esys_pcr_provider, allocate_returns_security_failure_when_platform_auth_unavailable)
{
    // Verifies platform authorization failures are translated for PCR allocate.

    fake::fake_esys_state state;
    state.pcr_allocate_rc = TPM2_RC_AUTH_FAIL;
    auto provider = make_provider(state);
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::security_failure);
}

TEST(esys_pcr_provider, allocate_handles_partial_allocation_result)
{
    // Verifies PCR allocate preserves a TPM-reported partial allocation result.

    fake::fake_esys_state state;
    state.allocation_success = TPM2_NO;
    state.max_pcr = 32U;
    state.size_needed = 90U;
    state.size_available = 11U;
    auto provider = make_provider(state);
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha512}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    EXPECT_FALSE(allocate->allocation_success);
    EXPECT_EQ(allocate->max_pcr, 32U);
    EXPECT_EQ(allocate->size_needed, 90U);
    EXPECT_EQ(allocate->size_available, 11U);
}

TEST(esys_pcr_provider, allocate_emits_success_log_fields)
{
    // Verifies PCR allocate success emits the full documented PCR log schema.

    fake::fake_esys_state state;
    state.allocation_success = TPM2_YES;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha384}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocate.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_success_log_schema(records.front(), events::pcr_allocate_completed);
    EXPECT_EQ(fake::field_value(records.front(), events::fields::bank_count), "1");
    EXPECT_EQ(fake::field_value(records.front(), events::fields::allocation_success), "true");
}

TEST(esys_pcr_provider, allocate_emits_pcr_tss_error_on_failure)
{
    // Verifies PCR allocate failures emit the PCR-specific TSS error event.

    fake::fake_esys_state state;
    state.pcr_allocate_rc = TPM2_RC_AUTH_FAIL;
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    auto provider = make_provider(state, *log);
    std::array<tpmkit::pcr::bank, 1U> banks{{tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto allocate = provider.allocate(gsl::make_span(banks));

    ASSERT_FALSE(allocate.has_value());
    const auto records = log->snapshot();
    ASSERT_EQ(records.size(), 1U);
    expect_error_log_schema(records.front(), events::pcr_tss_error,
                            events::values::security_failure, "pcr_allocate", "0x0000008e", "tpm");
}
