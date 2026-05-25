#include <tpmkit/testing/mock_pcr_provider.h>

#include <gtest/gtest.h>
#include <tl/expected.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace {

[[nodiscard]] tpmkit::pcr_digest_value make_sha256_digest(const std::uint8_t fill)
{
    return tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256,
                                    std::vector<std::uint8_t>(32U, fill)};
}

[[nodiscard]] tpmkit::pcr_read_result make_read_result()
{
    return tpmkit::pcr_read_result{
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}},
        7U,
        {make_sha256_digest(0xA5U)}};
}

[[nodiscard]] tpmkit::error make_test_error()
{
    return tpmkit::error{tpmkit::error_category::input_error, "programmed failure"};
}

} // namespace

TEST(mock_pcr_provider, default_response_returns_backend_error_for_each_operation)
{
    // Verifies every operation fails closed until a response is programmed.

    tpmkit::testing::mock_pcr_provider provider;
    const tpmkit::pcr_selection selection{tpmkit::hash_algorithm::sha256};
    const std::array<tpmkit::pcr_bank, 1> banks{{tpmkit::pcr_bank{tpmkit::hash_algorithm::sha256}}};
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x01U)}};
    const std::array<std::uint8_t, 2> event_data{{0x01U, 0x02U}};
    const std::array<std::uint8_t, 2> policy_digest{{0x03U, 0x04U}};

    const auto read = provider.read(selection);
    const auto extend = provider.extend(tpmkit::pcr_index::debug,
                                        gsl::span<const tpmkit::pcr_digest_value>{digests});
    const auto event =
        provider.event(tpmkit::pcr_index::debug, gsl::span<const std::uint8_t>{event_data});
    const auto reset = provider.reset(tpmkit::pcr_index::debug);
    const auto allocate = provider.allocate(gsl::span<const tpmkit::pcr_bank>{banks});
    const auto set_auth_value =
        provider.set_auth_value(tpmkit::pcr_index::debug, tpmkit::secret_buffer{});
    const auto set_auth_policy =
        provider.set_auth_policy(tpmkit::pcr_index::debug, tpmkit::hash_algorithm::sha256,
                                 gsl::span<const std::uint8_t>{policy_digest});

    ASSERT_FALSE(read.has_value());
    ASSERT_FALSE(extend.has_value());
    ASSERT_FALSE(event.has_value());
    ASSERT_FALSE(reset.has_value());
    ASSERT_FALSE(allocate.has_value());
    ASSERT_FALSE(set_auth_value.has_value());
    ASSERT_FALSE(set_auth_policy.has_value());
    EXPECT_EQ(read.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(extend.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(event.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(reset.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(allocate.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(set_auth_value.error().category, tpmkit::error_category::backend_error);
    EXPECT_EQ(set_auth_policy.error().category, tpmkit::error_category::backend_error);
}

TEST(mock_pcr_provider, programmed_success_response_is_returned_for_read)
{
    // Verifies read returns the caller-programmed successful read result.

    tpmkit::testing::mock_pcr_provider provider;
    const tpmkit::pcr_read_result expected = make_read_result();
    provider.set_read_result(expected);

    const auto actual = provider.read(
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}});

    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(actual.value(), expected);
}

TEST(mock_pcr_provider, programmed_success_response_is_returned_for_extend)
{
    // Verifies extend returns the caller-programmed successful empty outcome.

    tpmkit::testing::mock_pcr_provider provider;
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x02U)}};
    provider.set_extend_result({});

    const auto actual = provider.extend(tpmkit::pcr_index::debug,
                                        gsl::span<const tpmkit::pcr_digest_value>{digests});

    EXPECT_TRUE(actual.has_value());
}

TEST(mock_pcr_provider, programmed_error_response_is_returned_correctly)
{
    // Verifies programmed errors are returned without changing their category.

    tpmkit::testing::mock_pcr_provider provider;
    provider.set_event_result(tl::unexpected(make_test_error()));

    const auto actual = provider.event(tpmkit::pcr_index::debug, gsl::span<const std::uint8_t>{});

    ASSERT_FALSE(actual.has_value());
    EXPECT_EQ(actual.error().category, tpmkit::error_category::input_error);
    EXPECT_EQ(actual.error().message, "programmed failure");
}

TEST(mock_pcr_provider, tracks_call_count_for_each_operation)
{
    // Verifies each operation maintains an independent call counter.

    tpmkit::testing::mock_pcr_provider provider;
    const std::array<tpmkit::pcr_bank, 1> banks{{tpmkit::pcr_bank{tpmkit::hash_algorithm::sha256}}};
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x03U)}};

    static_cast<void>(provider.allocate(gsl::span<const tpmkit::pcr_bank>{banks}));
    static_cast<void>(provider.event(tpmkit::pcr_index::debug, gsl::span<const std::uint8_t>{}));
    static_cast<void>(provider.extend(tpmkit::pcr_index::debug,
                                      gsl::span<const tpmkit::pcr_digest_value>{digests}));
    static_cast<void>(provider.read(tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256}));
    static_cast<void>(provider.reset(tpmkit::pcr_index::debug));
    static_cast<void>(provider.set_auth_policy(
        tpmkit::pcr_index::debug, tpmkit::hash_algorithm::sha256, gsl::span<const std::uint8_t>{}));
    static_cast<void>(provider.set_auth_value(tpmkit::pcr_index::debug, tpmkit::secret_buffer{}));

    EXPECT_EQ(provider.allocate_call_count(), 1U);
    EXPECT_EQ(provider.event_call_count(), 1U);
    EXPECT_EQ(provider.extend_call_count(), 1U);
    EXPECT_EQ(provider.read_call_count(), 1U);
    EXPECT_EQ(provider.reset_call_count(), 1U);
    EXPECT_EQ(provider.set_auth_policy_call_count(), 1U);
    EXPECT_EQ(provider.set_auth_value_call_count(), 1U);

    provider.clear_call_counts();

    EXPECT_EQ(provider.allocate_call_count(), 0U);
    EXPECT_EQ(provider.event_call_count(), 0U);
    EXPECT_EQ(provider.extend_call_count(), 0U);
    EXPECT_EQ(provider.read_call_count(), 0U);
    EXPECT_EQ(provider.reset_call_count(), 0U);
    EXPECT_EQ(provider.set_auth_policy_call_count(), 0U);
    EXPECT_EQ(provider.set_auth_value_call_count(), 0U);
}
