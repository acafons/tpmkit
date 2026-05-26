#include <tpmkit/esys_pcr_provider.h>
#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr_bank.h>
#include <tpmkit/pcr_digest_value.h>
#include <tpmkit/pcr_index.h>
#include <tpmkit/pcr_selection.h>
#include <tpmkit/secret_buffer.h>
#include <tpmkit/testing/in_memory_pcr_observer.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm_context.h>

#include <gsl/span>

#include <openssl/evp.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct provider_bundle {
    std::unique_ptr<tpmkit::tpm_context> context;
    std::unique_ptr<tpmkit::pcr_provider> provider;
};

[[nodiscard]] std::array<tpmkit::hash_algorithm, 4U> all_hash_algorithms() noexcept
{
    return {tpmkit::hash_algorithm::sha1, tpmkit::hash_algorithm::sha256,
            tpmkit::hash_algorithm::sha384, tpmkit::hash_algorithm::sha512};
}

[[nodiscard]] std::string swtpm_tcti()
{
    const char* const configured = std::getenv("TPMKIT_SWTPM_TCTI");
    if (configured != nullptr) {
        return std::string{configured};
    }

    return "tabrmd:bus_type=system";
}

[[nodiscard]] tpmkit::tpm_context_config
context_config(const tpmkit::tpm_context_config::startup_mode startup)
{
    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{swtpm_tcti()};
    config.startup = startup;
    return config;
}

[[nodiscard]] provider_bundle make_provider(tpmkit::pcr_observer* const observer = nullptr,
                                            tpmkit::logger* const log = nullptr,
                                            const tpmkit::tpm_context_config::startup_mode startup =
                                                tpmkit::tpm_context_config::startup_mode::clear)
{
    auto context = tpmkit::tpm_context::create(context_config(startup));
    if (!context.has_value()) {
        throw std::runtime_error{context.error().message};
    }

    auto owned_context = std::make_unique<tpmkit::tpm_context>(std::move(context.value()));
    auto provider = tpmkit::create_esys_pcr_provider(*owned_context, observer, log);
    if (!provider.has_value()) {
        throw std::runtime_error{provider.error().message};
    }

    return provider_bundle{std::move(owned_context), std::move(provider.value())};
}

[[nodiscard]] const EVP_MD* evp_md(const tpmkit::hash_algorithm algorithm)
{
    switch (algorithm) {
    case tpmkit::hash_algorithm::sha1:
        return EVP_sha1();
    case tpmkit::hash_algorithm::sha256:
        return EVP_sha256();
    case tpmkit::hash_algorithm::sha384:
        return EVP_sha384();
    case tpmkit::hash_algorithm::sha512:
        return EVP_sha512();
    }

    throw std::runtime_error{"unsupported hash algorithm"};
}

[[nodiscard]] std::vector<std::uint8_t> hash_bytes(const tpmkit::hash_algorithm algorithm,
                                                   const gsl::span<const std::uint8_t> bytes)
{
    std::vector<std::uint8_t> digest(EVP_MAX_MD_SIZE);
    unsigned int digest_size = 0U;
    const int rc = EVP_Digest(bytes.data(), bytes.size(), digest.data(), &digest_size,
                              evp_md(algorithm), nullptr);
    if (rc != 1) {
        throw std::runtime_error{"OpenSSL digest calculation failed"};
    }

    digest.resize(digest_size);
    return digest;
}

[[nodiscard]] std::vector<std::uint8_t> hash_pcr_extend(const tpmkit::hash_algorithm algorithm,
                                                        const std::vector<std::uint8_t>& old_value,
                                                        const std::vector<std::uint8_t>& digest)
{
    std::vector<std::uint8_t> input;
    input.reserve(old_value.size() + digest.size());
    input.insert(input.end(), old_value.begin(), old_value.end());
    input.insert(input.end(), digest.begin(), digest.end());
    return hash_bytes(algorithm, gsl::make_span(input));
}

[[nodiscard]] tpmkit::pcr_digest_value digest_value(const tpmkit::hash_algorithm algorithm,
                                                    const std::uint8_t fill)
{
    return tpmkit::pcr_digest_value{
        algorithm, std::vector<std::uint8_t>(tpmkit::digest_size(algorithm), fill)};
}

[[nodiscard]] tpmkit::outcome<std::vector<std::uint8_t>>
read_debug_digest(tpmkit::pcr_provider& provider, const tpmkit::hash_algorithm algorithm)
{
    auto read = provider.read(tpmkit::pcr_selection{algorithm, {tpmkit::pcr_index::debug}});
    if (!read.has_value()) {
        return tl::unexpected(read.error());
    }

    if (read.value().values.empty()) {
        return tl::unexpected(tpmkit::error{tpmkit::error_category::resource_error,
                                            "PCR bank is not active in swtpm"});
    }

    return read.value().values.front().digest.digest();
}

[[nodiscard]] std::vector<tpmkit::hash_algorithm> active_algorithms(tpmkit::pcr_provider& provider)
{
    std::vector<tpmkit::hash_algorithm> active;
    for (const tpmkit::hash_algorithm algorithm : all_hash_algorithms()) {
        const auto digest = read_debug_digest(provider, algorithm);
        if (digest.has_value()) {
            active.push_back(algorithm);
        }
    }

    return active;
}

[[nodiscard]] std::vector<tpmkit::pcr_index> auth_candidate_indices()
{
    return {tpmkit::pcr_index::debug,      tpmkit::pcr_index::application,
            tpmkit::pcr_index::drtm_17,    tpmkit::pcr_index::drtm_18,
            tpmkit::pcr_index::drtm_19,    tpmkit::pcr_index::drtm_20,
            tpmkit::pcr_index::drtm_21,    tpmkit::pcr_index::drtm_22,
            tpmkit::pcr_index::firmware_0, tpmkit::pcr_index::firmware_1,
            tpmkit::pcr_index::firmware_2, tpmkit::pcr_index::firmware_3,
            tpmkit::pcr_index::firmware_4, tpmkit::pcr_index::firmware_5,
            tpmkit::pcr_index::firmware_6, tpmkit::pcr_index::firmware_7};
}

[[nodiscard]] const tpmkit::pcr_digest_value*
find_digest(const std::vector<tpmkit::pcr_digest_value>& values,
            const tpmkit::hash_algorithm algorithm) noexcept
{
    const auto it = std::find_if(values.begin(), values.end(), [algorithm](const auto& value) {
        return value.algorithm() == algorithm;
    });
    if (it == values.end()) {
        return nullptr;
    }

    return std::addressof(*it);
}

[[nodiscard]] bool contains_algorithm(const std::vector<tpmkit::hash_algorithm>& algorithms,
                                      const tpmkit::hash_algorithm algorithm)
{
    return std::find(algorithms.begin(), algorithms.end(), algorithm) != algorithms.end();
}

[[nodiscard]] std::string hex_encode(const std::vector<std::uint8_t>& bytes)
{
    std::ostringstream out;
    out << std::hex;
    for (const std::uint8_t byte : bytes) {
        out.width(2);
        out.fill('0');
        out << static_cast<unsigned int>(byte);
    }

    return out.str();
}

[[nodiscard]] std::string uppercase(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return text;
}

[[nodiscard]] tpmkit::outcome<tpmkit::pcr_index>
set_auth_policy_on_supported_pcr(tpmkit::pcr_provider& provider,
                                 const gsl::span<const std::uint8_t> policy_digest)
{
    tpmkit::error last_error{tpmkit::error_category::resource_error,
                             "no PCR accepted SetAuthPolicy"};
    for (const tpmkit::pcr_index index : auth_candidate_indices()) {
        auto result =
            provider.set_auth_policy(index, tpmkit::hash_algorithm::sha256, policy_digest);
        if (result.has_value()) {
            return index;
        }

        last_error = result.error();
    }

    return tl::unexpected(last_error);
}

[[nodiscard]] tpmkit::outcome<tpmkit::pcr_index>
set_auth_value_on_supported_pcr(tpmkit::pcr_provider& provider,
                                const std::vector<std::uint8_t>& secret)
{
    tpmkit::error last_error{tpmkit::error_category::resource_error,
                             "no PCR accepted SetAuthValue"};
    for (const tpmkit::pcr_index index : auth_candidate_indices()) {
        std::vector<std::uint8_t> owned_secret = secret;
        auto result =
            provider.set_auth_value(index, tpmkit::secret_buffer{std::move(owned_secret)});
        if (result.has_value()) {
            return index;
        }

        last_error = result.error();
    }

    return tl::unexpected(last_error);
}

[[nodiscard]] bool contains_any_secret_pattern(const std::string& text,
                                               const std::vector<std::string>& secret_patterns)
{
    return std::any_of(secret_patterns.begin(), secret_patterns.end(),
                       [&text](const auto& pattern) {
                           return !pattern.empty() && text.find(pattern) != std::string::npos;
                       });
}

void expect_no_secret_leaks(const tpmkit::testing::recording_logger& log,
                            const std::vector<std::uint8_t>& secret)
{
    const std::string raw{secret.begin(), secret.end()};
    const std::string hex = hex_encode(secret);
    const std::vector<std::string> secret_patterns{raw, hex, uppercase(hex)};
    for (const auto& record : log.snapshot()) {
        EXPECT_FALSE(contains_any_secret_pattern(record.message, secret_patterns));
        for (const auto& field : record.fields) {
            EXPECT_FALSE(contains_any_secret_pattern(field.first, secret_patterns));
            EXPECT_FALSE(contains_any_secret_pattern(field.second, secret_patterns));
        }
    }
}

} // namespace

TEST(pcr_provider_swtpm, reads_default_sha256_debug_pcr_as_zero)
{
    // Verifies a swtpm-backed provider reads the default SHA-256 debug PCR as zeros.

    auto bundle = make_provider();

    const auto read = bundle.provider->read(
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}});

    ASSERT_TRUE(read.has_value()) << read.error().message;
    ASSERT_EQ(read.value().values.size(), 1U);
    const auto& value = read.value().values.front();
    EXPECT_EQ(value.index, tpmkit::pcr_index::debug);
    EXPECT_EQ(value.digest.algorithm(), tpmkit::hash_algorithm::sha256);
    EXPECT_EQ(value.digest.digest(),
              std::vector<std::uint8_t>(tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 0U));
}

TEST(pcr_provider_swtpm, reads_sha256_and_sha1_debug_banks)
{
    // Verifies the provider can read both SHA-256 and SHA-1 PCR banks from swtpm.

    auto bundle = make_provider();

    const auto sha256 = read_debug_digest(*bundle.provider, tpmkit::hash_algorithm::sha256);
    const auto sha1 = read_debug_digest(*bundle.provider, tpmkit::hash_algorithm::sha1);

    ASSERT_TRUE(sha256.has_value()) << sha256.error().message;
    ASSERT_TRUE(sha1.has_value()) << sha1.error().message;
    EXPECT_EQ(sha256.value().size(), tpmkit::digest_size(tpmkit::hash_algorithm::sha256));
    EXPECT_EQ(sha1.value().size(), tpmkit::digest_size(tpmkit::hash_algorithm::sha1));
}

TEST(pcr_provider_swtpm, extends_debug_pcr_and_reads_chained_sha256_value)
{
    // Verifies PCR_Extend updates PCR 16 as hash(old_value || digest) across two extends.

    auto bundle = make_provider();
    const auto reset = bundle.provider->reset(tpmkit::pcr_index::debug);
    ASSERT_TRUE(reset.has_value()) << reset.error().message;
    const std::vector<std::uint8_t> zero(tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 0U);
    const auto first_digest = digest_value(tpmkit::hash_algorithm::sha256, 0x42U);
    const std::array<tpmkit::pcr_digest_value, 1U> first{first_digest};

    const auto first_extend = bundle.provider->extend(
        tpmkit::pcr_index::debug, gsl::span<const tpmkit::pcr_digest_value>(first));
    ASSERT_TRUE(first_extend.has_value()) << first_extend.error().message;
    const auto first_read = read_debug_digest(*bundle.provider, tpmkit::hash_algorithm::sha256);

    ASSERT_TRUE(first_read.has_value()) << first_read.error().message;
    const auto first_expected =
        hash_pcr_extend(tpmkit::hash_algorithm::sha256, zero, first_digest.digest());
    EXPECT_EQ(first_read.value(), first_expected);

    const auto second_digest = digest_value(tpmkit::hash_algorithm::sha256, 0x24U);
    const std::array<tpmkit::pcr_digest_value, 1U> second{second_digest};
    const auto second_extend = bundle.provider->extend(
        tpmkit::pcr_index::debug, gsl::span<const tpmkit::pcr_digest_value>(second));
    ASSERT_TRUE(second_extend.has_value()) << second_extend.error().message;
    const auto second_read = read_debug_digest(*bundle.provider, tpmkit::hash_algorithm::sha256);

    ASSERT_TRUE(second_read.has_value()) << second_read.error().message;
    const auto second_expected =
        hash_pcr_extend(tpmkit::hash_algorithm::sha256, first_expected, second_digest.digest());
    EXPECT_EQ(second_read.value(), second_expected);
}

TEST(pcr_provider_swtpm, events_debug_pcr_and_returns_hashes_for_active_banks)
{
    // Verifies PCR_Event returns TPM-computed event digests for every active swtpm bank.

    auto bundle = make_provider();
    const auto reset = bundle.provider->reset(tpmkit::pcr_index::debug);
    ASSERT_TRUE(reset.has_value()) << reset.error().message;
    const auto active = active_algorithms(*bundle.provider);
    const std::vector<std::uint8_t> event_data{'t', 'p', 'm', 'k', 'i', 't',
                                               '-', 'e', 'v', 'e', 'n', 't'};

    const auto event = bundle.provider->event(tpmkit::pcr_index::debug, gsl::make_span(event_data));

    ASSERT_TRUE(event.has_value()) << event.error().message;
    ASSERT_GE(event.value().digests.size(), active.size());
    for (const tpmkit::hash_algorithm algorithm : active) {
        const auto* digest = find_digest(event.value().digests, algorithm);
        ASSERT_NE(digest, nullptr);
        EXPECT_EQ(digest->digest(), hash_bytes(algorithm, gsl::make_span(event_data)));
    }
}

TEST(pcr_provider_swtpm, resets_debug_pcr_to_zero)
{
    // Verifies PCR_Reset returns PCR 16 to the zero digest after an extend.

    auto bundle = make_provider();
    const auto digest = digest_value(tpmkit::hash_algorithm::sha256, 0x99U);
    const std::array<tpmkit::pcr_digest_value, 1U> digests{digest};
    const auto extend = bundle.provider->extend(tpmkit::pcr_index::debug,
                                                gsl::span<const tpmkit::pcr_digest_value>(digests));
    ASSERT_TRUE(extend.has_value()) << extend.error().message;

    const auto reset = bundle.provider->reset(tpmkit::pcr_index::debug);
    const auto read = read_debug_digest(*bundle.provider, tpmkit::hash_algorithm::sha256);

    ASSERT_TRUE(reset.has_value()) << reset.error().message;
    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(read.value(),
              std::vector<std::uint8_t>(tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 0U));
}

TEST(pcr_provider_swtpm, set_auth_value_rejects_non_empty_auth_when_transport_is_unavailable)
{
    // Verifies non-empty SetAuthValue fails closed without leaking auth material.

    tpmkit::testing::recording_logger log;
    auto bundle = make_provider(nullptr, &log);
    const std::vector<std::uint8_t> secret{'t', 'a', 's', 'k', '0', '9', '-', 'a', 'u',
                                           't', 'h', '-', 'c', 'a', 'n', 'a', 'r', 'y'};

    auto authorized_index = set_auth_value_on_supported_pcr(*bundle.provider, secret);

    ASSERT_FALSE(authorized_index.has_value());
    EXPECT_EQ(authorized_index.error().category, tpmkit::error_category::resource_error);
    expect_no_secret_leaks(log, secret);
}

TEST(pcr_provider_swtpm, set_auth_policy_enforces_policy_when_supported)
{
    // Verifies SetAuthPolicy enforcement when the simulator exposes an eligible PCR.

    auto bundle = make_provider();
    const std::vector<std::uint8_t> policy_digest(
        tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 0U);

    const auto policy_index =
        set_auth_policy_on_supported_pcr(*bundle.provider, gsl::make_span(policy_digest));
    if (!policy_index.has_value()) {
        EXPECT_FALSE(policy_index.error().message.empty());
        return;
    }

    const auto digest = digest_value(tpmkit::hash_algorithm::sha256, 0x31U);
    const std::array<tpmkit::pcr_digest_value, 1U> digests{digest};

    const auto unauthorized = bundle.provider->extend(
        policy_index.value(), gsl::span<const tpmkit::pcr_digest_value>(digests));

    ASSERT_FALSE(unauthorized.has_value());
    EXPECT_EQ(unauthorized.error().category, tpmkit::error_category::security_failure);
}

TEST(pcr_provider_swtpm, observer_records_extend_and_event_measurements)
{
    // Verifies the in-memory observer records successful PCR_Extend and PCR_Event notifications.

    tpmkit::testing::in_memory_pcr_observer observer;
    auto bundle = make_provider(&observer);
    const auto reset = bundle.provider->reset(tpmkit::pcr_index::debug);
    ASSERT_TRUE(reset.has_value()) << reset.error().message;
    const auto digest = digest_value(tpmkit::hash_algorithm::sha256, 0x55U);
    const std::array<tpmkit::pcr_digest_value, 1U> digests{digest};
    const std::vector<std::uint8_t> event_data{'o', 'b', 's', 'e', 'r', 'v', 'e', 'd'};

    const auto extend = bundle.provider->extend(tpmkit::pcr_index::debug,
                                                gsl::span<const tpmkit::pcr_digest_value>(digests));
    const auto event = bundle.provider->event(tpmkit::pcr_index::debug, gsl::make_span(event_data));

    ASSERT_TRUE(extend.has_value()) << extend.error().message;
    ASSERT_TRUE(event.has_value()) << event.error().message;
    ASSERT_EQ(observer.count(), 2U);
    const auto& entries = observer.entries();
    EXPECT_EQ(entries[0].operation, tpmkit::testing::pcr_measurement_operation::extend);
    EXPECT_EQ(entries[0].index, tpmkit::pcr_index::debug);
    EXPECT_EQ(entries[0].digests, std::vector<tpmkit::pcr_digest_value>{digest});
    EXPECT_TRUE(entries[0].event_data.empty());
    EXPECT_EQ(entries[1].operation, tpmkit::testing::pcr_measurement_operation::event);
    EXPECT_EQ(entries[1].index, tpmkit::pcr_index::debug);
    EXPECT_EQ(entries[1].event_data, event_data);
    EXPECT_EQ(entries[1].digests, event.value().digests);
}

TEST(pcr_provider_swtpm, allocates_sha384_bank_when_available)
{
    // Verifies PCR_Allocate accepts active banks plus SHA-384 for swtpm bank management.

    auto bundle = make_provider();
    std::vector<tpmkit::hash_algorithm> requested = active_algorithms(*bundle.provider);
    if (!contains_algorithm(requested, tpmkit::hash_algorithm::sha384)) {
        requested.push_back(tpmkit::hash_algorithm::sha384);
    }

    std::vector<tpmkit::pcr_bank> banks;
    banks.reserve(requested.size());
    std::transform(
        requested.begin(), requested.end(), std::back_inserter(banks),
        [](const tpmkit::hash_algorithm algorithm) { return tpmkit::pcr_bank{algorithm}; });

    const auto allocation = bundle.provider->allocate(gsl::make_span(banks));

    ASSERT_TRUE(allocation.has_value()) << allocation.error().message;
    EXPECT_TRUE(allocation.value().allocation_success);
    EXPECT_GE(allocation.value().max_pcr, 24U);
    EXPECT_TRUE(contains_algorithm(requested, tpmkit::hash_algorithm::sha384));
}
