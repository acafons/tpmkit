#include <tpmkit/hash_algorithm.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
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

    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha1), 20U);
    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha256), 32U);
    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha384), 48U);
    EXPECT_EQ(tpmkit::digest_size(tpmkit::hash_algorithm::sha512), 64U);
}

} // namespace
