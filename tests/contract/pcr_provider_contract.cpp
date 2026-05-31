#include <tpmkit/pcr/provider.h>
#include <tpmkit/testing/fake_tcti.h>
#include <tpmkit/testing/mock_pcr_provider.h>
#include <tpmkit/tpm2_esys/owned_tcti_context.h>
#include <tpmkit/tpm_context.h>

#include <gtest/gtest.h>
#include <tl/expected.hpp>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace {

#if defined(__GNUC__) || defined(__clang__)
// TSS constants in public headers intentionally expand through C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

using startup_mode = tpmkit::tpm_context_config::startup_mode;

struct provider_subject {
    std::unique_ptr<tpmkit::testing::fake_tcti> fake;
    std::unique_ptr<tpmkit::tpm_context> context;
    std::unique_ptr<tpmkit::pcr::provider> provider;
};

struct pcr_provider_case {
    const char* name;
    std::function<provider_subject()> make_read_success;
    std::function<provider_subject()> make_extend_success;
    std::function<provider_subject()> make_event_success;
    std::function<provider_subject()> make_reset_success;
    std::function<provider_subject()> make_allocate_success;
    std::function<provider_subject()> make_auth_policy_success;
    std::function<provider_subject()> make_auth_value_success;
    std::function<provider_subject()> make_resource_failure;
    std::function<tpmkit::error(tpmkit::pcr::provider&)> exercise_resource_failure;
    std::function<provider_subject()> make_backend_failure;
    std::function<tpmkit::error(tpmkit::pcr::provider&)> exercise_backend_failure;
    std::function<provider_subject()> make_security_failure;
    std::function<tpmkit::error(tpmkit::pcr::provider&)> exercise_security_failure;
};

void PrintTo(const pcr_provider_case& test_case, std::ostream* const output)
{
    *output << test_case.name;
}

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

std::vector<std::uint8_t> error_response(const std::uint32_t rc)
{
    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_NO_SESSIONS, 10U, rc);
    return response;
}

std::vector<std::uint8_t> session_success_response()
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

std::vector<std::uint8_t> allocate_success_response()
{
    std::vector<std::uint8_t> parameters;
    append_u8(parameters, TPM2_YES);
    append_u32(parameters, 32U);
    append_u32(parameters, 12U);
    append_u32(parameters, 44U);

    std::vector<std::uint8_t> response;
    append_header(response, TPM2_ST_SESSIONS,
                  static_cast<std::uint32_t>(10U + 4U + parameters.size() + 5U), TSS2_RC_SUCCESS);
    append_u32(response, static_cast<std::uint32_t>(parameters.size()));
    response.insert(response.end(), parameters.begin(), parameters.end());
    append_empty_auth_response(response);
    return response;
}

tpmkit::tpm2_esys::owned_tcti_context owned_tcti(tpmkit::testing::fake_tcti& fake)
{
    return tpmkit::tpm2_esys::owned_tcti_context{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(fake.handle(),
                                                                         finalize_tcti_handle)};
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

tpmkit::pcr::read_result expected_read_result()
{
    return tpmkit::pcr::read_result{
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}},
        9U,
        {tpmkit::pcr::value{tpmkit::pcr::index::debug, sha256_digest(0x20U)}}};
}

tpmkit::error make_error(const tpmkit::error_category category)
{
    return tpmkit::error{category, "contract failure"};
}

provider_subject make_mock_read_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_read_result(expected_read_result());
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_extend_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_extend_result({});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_event_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_event_result(tpmkit::pcr::event_result{{sha256_digest(0x30U)}});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_reset_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_reset_result({});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_allocate_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_allocate_result(tpmkit::pcr::allocate_result{true, 32U, 12U, 44U});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_auth_policy_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_auth_policy_result({});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_auth_value_success()
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_auth_value_result({});
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_with_reset_failure(const tpmkit::error_category category)
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_reset_result(tl::unexpected(make_error(category)));
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_with_read_failure(const tpmkit::error_category category)
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_read_result(tl::unexpected(make_error(category)));
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_mock_with_auth_policy_failure(const tpmkit::error_category category)
{
    provider_subject subject;
    auto provider = std::make_unique<tpmkit::testing::mock_pcr_provider>();
    provider->set_auth_policy_result(tl::unexpected(make_error(category)));
    subject.provider = std::move(provider);
    return subject;
}

provider_subject make_esys_subject(std::vector<std::uint8_t> response)
{
    provider_subject subject;
    subject.fake = std::make_unique<tpmkit::testing::fake_tcti>();
    subject.fake->push_response(std::move(response));

    auto context = tpmkit::tpm2_esys::create_context(owned_tcti(*subject.fake),
                                                     startup_mode::skip);
    if (!context.has_value()) {
        ADD_FAILURE() << context.error().message;
        return subject;
    }

    subject.context = std::make_unique<tpmkit::tpm_context>(std::move(context.value()));
    auto provider = subject.context->create_pcr_provider();
    if (!provider.has_value()) {
        ADD_FAILURE() << provider.error().message;
        return subject;
    }

    subject.provider = std::move(provider.value());
    return subject;
}

provider_subject make_esys_read_success()
{
    return make_esys_subject(
        read_success_response(9U, tpmkit::pcr::index::debug.value(), digest_bytes(0x20U)));
}

provider_subject make_esys_extend_success()
{
    return make_esys_subject(session_success_response());
}

provider_subject make_esys_event_success()
{
    return make_esys_subject(event_success_response(digest_bytes(0x30U)));
}

provider_subject make_esys_reset_success()
{
    return make_esys_subject(session_success_response());
}

provider_subject make_esys_allocate_success()
{
    return make_esys_subject(allocate_success_response());
}

provider_subject make_esys_auth_policy_success()
{
    return make_esys_subject(session_success_response());
}

provider_subject make_esys_auth_value_success()
{
    return make_esys_subject(session_success_response());
}

provider_subject make_esys_resource_failure()
{
    return make_esys_subject(error_response(TPM2_RC_LOCALITY));
}

provider_subject make_esys_backend_failure()
{
    return make_esys_subject(error_response(TPM2_RC_INITIALIZE));
}

provider_subject make_esys_security_failure()
{
    return make_esys_subject(error_response(TPM2_RC_AUTH_FAIL));
}

tpmkit::error reset_and_return_error(tpmkit::pcr::provider& provider)
{
    const auto result = provider.reset(tpmkit::pcr::index::firmware_0);
    if (result.has_value()) {
        return make_error(tpmkit::error_category::backend_error);
    }
    return result.error();
}

tpmkit::error read_and_return_error(tpmkit::pcr::provider& provider)
{
    const auto result = provider.read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});
    if (result.has_value()) {
        return make_error(tpmkit::error_category::backend_error);
    }
    return result.error();
}

tpmkit::error set_auth_policy_and_return_error(tpmkit::pcr::provider& provider)
{
    const auto policy = digest_bytes(0x70U);
    const auto result =
        provider.set_auth_policy(tpmkit::pcr::index::debug, tpmkit::hash_algorithm::sha256, policy);
    if (result.has_value()) {
        return make_error(tpmkit::error_category::backend_error);
    }
    return result.error();
}

std::vector<pcr_provider_case> provider_cases()
{
    return {
        pcr_provider_case{
            "mock_pcr_provider",
            make_mock_read_success,
            make_mock_extend_success,
            make_mock_event_success,
            make_mock_reset_success,
            make_mock_allocate_success,
            make_mock_auth_policy_success,
            make_mock_auth_value_success,
            [] { return make_mock_with_reset_failure(tpmkit::error_category::resource_error); },
            reset_and_return_error,
            [] { return make_mock_with_read_failure(tpmkit::error_category::backend_error); },
            read_and_return_error,
            [] {
                return make_mock_with_auth_policy_failure(tpmkit::error_category::security_failure);
            },
            set_auth_policy_and_return_error,
        },
        pcr_provider_case{
            "tpm2_esys_fake_tcti",
            make_esys_read_success,
            make_esys_extend_success,
            make_esys_event_success,
            make_esys_reset_success,
            make_esys_allocate_success,
            make_esys_auth_policy_success,
            make_esys_auth_value_success,
            make_esys_resource_failure,
            reset_and_return_error,
            make_esys_backend_failure,
            read_and_return_error,
            make_esys_security_failure,
            set_auth_policy_and_return_error,
        },
    };
}

std::string provider_case_name(const testing::TestParamInfo<pcr_provider_case>& info)
{
    return info.param.name;
}

class pcr_provider_contract : public testing::TestWithParam<pcr_provider_case> {};

TEST_P(pcr_provider_contract, reads_selected_pcr_values)
{
    // Verifies provider implementations return PCR read results through the port contract.

    provider_subject subject = GetParam().make_read_success();
    ASSERT_NE(subject.provider, nullptr);

    const auto result = subject.provider->read(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expected_read_result());
}

TEST_P(pcr_provider_contract, extends_one_digest)
{
    // Verifies provider implementations accept one explicit digest for PCR extend.

    provider_subject subject = GetParam().make_extend_success();
    ASSERT_NE(subject.provider, nullptr);
    const tpmkit::pcr::digest_value digest = sha256_digest(0x40U);

    const auto result =
        subject.provider->extend(tpmkit::pcr::index::debug, gsl::make_span(&digest, 1U));

    EXPECT_TRUE(result.has_value());
}

TEST_P(pcr_provider_contract, events_raw_measurement_bytes)
{
    // Verifies provider implementations return event digests for raw PCR event bytes.

    provider_subject subject = GetParam().make_event_success();
    ASSERT_NE(subject.provider, nullptr);
    const std::array<std::uint8_t, 2U> event_data{{0x01U, 0x02U}};

    const auto result =
        subject.provider->event(tpmkit::pcr::index::debug, gsl::make_span(event_data));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->digests.size(), 1U);
    EXPECT_EQ(result->digests.front(), sha256_digest(0x30U));
}

TEST_P(pcr_provider_contract, resets_resettable_pcr)
{
    // Verifies provider implementations accept reset for the resettable debug PCR.

    provider_subject subject = GetParam().make_reset_success();
    ASSERT_NE(subject.provider, nullptr);

    const auto result = subject.provider->reset(tpmkit::pcr::index::debug);

    EXPECT_TRUE(result.has_value());
}

TEST_P(pcr_provider_contract, allocates_requested_banks)
{
    // Verifies provider implementations return PCR allocation status fields.

    provider_subject subject = GetParam().make_allocate_success();
    ASSERT_NE(subject.provider, nullptr);
    const std::array<tpmkit::pcr::bank, 1U> banks{
        {tpmkit::pcr::bank{tpmkit::hash_algorithm::sha256}}};

    const auto result = subject.provider->allocate(gsl::make_span(banks));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->allocation_success);
    EXPECT_EQ(result->max_pcr, 32U);
    EXPECT_EQ(result->size_needed, 12U);
    EXPECT_EQ(result->size_available, 44U);
}

TEST_P(pcr_provider_contract, sets_auth_policy_for_supported_algorithm)
{
    // Verifies provider implementations accept SHA-256 PCR auth policy digests.

    provider_subject subject = GetParam().make_auth_policy_success();
    ASSERT_NE(subject.provider, nullptr);
    const auto policy = digest_bytes(0x50U);

    const auto result = subject.provider->set_auth_policy(tpmkit::pcr::index::debug,
                                                          tpmkit::hash_algorithm::sha256, policy);

    EXPECT_TRUE(result.has_value());
}

TEST_P(pcr_provider_contract, sets_empty_auth_value)
{
    // Verifies provider implementations accept an empty PCR auth value.

    provider_subject subject = GetParam().make_auth_value_success();
    ASSERT_NE(subject.provider, nullptr);

    const auto result =
        subject.provider->set_auth_value(tpmkit::pcr::index::debug, tpmkit::secret_buffer{});

    EXPECT_TRUE(result.has_value());
}

TEST_P(pcr_provider_contract, surfaces_resource_errors)
{
    // Verifies provider implementations surface unavailable-resource failures consistently.

    provider_subject subject = GetParam().make_resource_failure();
    ASSERT_NE(subject.provider, nullptr);

    const tpmkit::error error = GetParam().exercise_resource_failure(*subject.provider);

    EXPECT_EQ(error.category, tpmkit::error_category::resource_error);
}

TEST_P(pcr_provider_contract, surfaces_backend_errors)
{
    // Verifies provider implementations surface opaque backend failures consistently.

    provider_subject subject = GetParam().make_backend_failure();
    ASSERT_NE(subject.provider, nullptr);

    const tpmkit::error error = GetParam().exercise_backend_failure(*subject.provider);

    EXPECT_EQ(error.category, tpmkit::error_category::backend_error);
}

TEST_P(pcr_provider_contract, surfaces_security_failures)
{
    // Verifies provider implementations surface authorization failures as security failures.

    provider_subject subject = GetParam().make_security_failure();
    ASSERT_NE(subject.provider, nullptr);

    const tpmkit::error error = GetParam().exercise_security_failure(*subject.provider);

    EXPECT_EQ(error.category, tpmkit::error_category::security_failure);
}

INSTANTIATE_TEST_SUITE_P(provider_adapters, pcr_provider_contract,
                         testing::ValuesIn(provider_cases()), provider_case_name);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // namespace
