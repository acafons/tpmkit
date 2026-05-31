#include <tpmkit/hash_algorithm.h>
#include <tpmkit/exception.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace {

TEST(hash_algorithm, exposes_exact_supported_algorithm_set)
{
    // Verifies the public hash algorithm vocabulary contains the expected cases.

    constexpr std::array<tpmkit::hash_algorithm, 4U> algorithms{{
        tpmkit::hash_algorithm::sha1,
        tpmkit::hash_algorithm::sha256,
        tpmkit::hash_algorithm::sha384,
        tpmkit::hash_algorithm::sha512,
    }};

    EXPECT_EQ(algorithms.size(), 4U);
    EXPECT_EQ(static_cast<std::size_t>(tpmkit::hash_algorithm::sha1), 0U);
    EXPECT_EQ(static_cast<std::size_t>(tpmkit::hash_algorithm::sha256), 1U);
    EXPECT_EQ(static_cast<std::size_t>(tpmkit::hash_algorithm::sha384), 2U);
    EXPECT_EQ(static_cast<std::size_t>(tpmkit::hash_algorithm::sha512), 3U);
    EXPECT_TRUE(std::is_enum<tpmkit::hash_algorithm>::value);
}

TEST(hash_algorithm, reports_digest_size_for_each_supported_algorithm)
{
    // Verifies digest_size maps every public algorithm to its byte length.

    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 32U);
    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha384), 48U);
    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha512), 64U);
}

TEST(hash_algorithm, rejects_sha1_digest_size_by_default)
{
    // Verifies SHA-1 PCR compatibility is gated behind an explicit build option.

    EXPECT_THROW(static_cast<void>(tpmkit::digest_size(tpmkit::hash_algorithm::sha1)),
                 tpmkit::tpmkit_error);
}

TEST(hash_algorithm, reports_stable_name_for_each_supported_algorithm)
{
    // Verifies hash_algorithm_name maps every public algorithm to its canonical text.

    EXPECT_EQ(tpmkit::hash_algorithm_name(tpmkit::hash_algorithm::sha1), std::string_view{"sha1"});
    EXPECT_EQ(tpmkit::hash_algorithm_name(tpmkit::hash_algorithm::sha256),
              std::string_view{"sha256"});
    EXPECT_EQ(tpmkit::hash_algorithm_name(tpmkit::hash_algorithm::sha384),
              std::string_view{"sha384"});
    EXPECT_EQ(tpmkit::hash_algorithm_name(tpmkit::hash_algorithm::sha512),
              std::string_view{"sha512"});
}

TEST(hash_algorithm, reports_unknown_name_for_unsupported_algorithm)
{
    // Verifies diagnostics can safely name an invalid enum value.

    EXPECT_EQ(tpmkit::hash_algorithm_name(static_cast<tpmkit::hash_algorithm>(99)),
              std::string_view{"unknown"});
}

} // namespace
