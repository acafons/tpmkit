#include <tpmkit/exception.h>
#include <tpmkit/pcr_bank.h>
#include <tpmkit/pcr_digest_value.h>
#include <tpmkit/pcr_index.h>
#include <tpmkit/pcr_result_types.h>
#include <tpmkit/pcr_selection.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::uint8_t> digest_bytes(const std::size_t size,
                                                     const std::uint8_t fill = 0xA5U)
{
    return std::vector<std::uint8_t>(size, fill);
}

[[nodiscard]] const char* algorithm_name(const tpmkit::hash_algorithm algorithm) noexcept
{
    switch (algorithm) {
    case tpmkit::hash_algorithm::sha1:
        return "sha1";
    case tpmkit::hash_algorithm::sha256:
        return "sha256";
    case tpmkit::hash_algorithm::sha384:
        return "sha384";
    case tpmkit::hash_algorithm::sha512:
        return "sha512";
    }

    return "unsupported";
}

} // namespace

namespace tpmkit {

void PrintTo(const pcr_index& index, std::ostream* const os)
{
    *os << "pcr_index{" << static_cast<unsigned int>(index.value()) << "}";
}

void PrintTo(const pcr_bank& bank, std::ostream* const os)
{
    *os << "pcr_bank{" << algorithm_name(bank.algorithm()) << ", " << bank.digest_size()
        << " bytes}";
}

void PrintTo(const pcr_digest_value& value, std::ostream* const os)
{
    *os << "pcr_digest_value{" << algorithm_name(value.algorithm()) << ", <redacted, "
        << value.digest().size() << " bytes>}";
}

void PrintTo(const pcr_selection& selection, std::ostream* const os)
{
    *os << "pcr_selection{" << algorithm_name(selection.algorithm()) << ", "
        << selection.indices().size() << " indices}";
}

} // namespace tpmkit

namespace {

TEST(pcr_index, constructs_valid_indices)
{
    // Verifies every TPM PCR index in the supported range can be constructed.

    for (std::uint32_t value = 0U; value <= tpmkit::pcr_index::max_value; ++value) {
        const tpmkit::pcr_index index{value};

        EXPECT_EQ(index.value(), value);
    }
}

TEST(pcr_index, rejects_indices_above_supported_range)
{
    // Verifies out-of-range PCR indices fail during construction.

    EXPECT_THROW(static_cast<void>(tpmkit::pcr_index{32U}), tpmkit::input_validation_error);
    EXPECT_THROW(static_cast<void>(tpmkit::pcr_index{255U}), tpmkit::input_validation_error);
}

TEST(pcr_index, exposes_well_known_named_constants)
{
    // Verifies named PCR constants map to the expected platform roles.

    EXPECT_EQ(tpmkit::pcr_index::firmware_0.value(), 0U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_1.value(), 1U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_2.value(), 2U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_3.value(), 3U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_4.value(), 4U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_5.value(), 5U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_6.value(), 6U);
    EXPECT_EQ(tpmkit::pcr_index::firmware_7.value(), 7U);
    EXPECT_EQ(tpmkit::pcr_index::bootloader_8.value(), 8U);
    EXPECT_EQ(tpmkit::pcr_index::bootloader_9.value(), 9U);
    EXPECT_EQ(tpmkit::pcr_index::ima.value(), 10U);
    EXPECT_EQ(tpmkit::pcr_index::os_11.value(), 11U);
    EXPECT_EQ(tpmkit::pcr_index::os_12.value(), 12U);
    EXPECT_EQ(tpmkit::pcr_index::os_13.value(), 13U);
    EXPECT_EQ(tpmkit::pcr_index::os_14.value(), 14U);
    EXPECT_EQ(tpmkit::pcr_index::os_15.value(), 15U);
    EXPECT_EQ(tpmkit::pcr_index::debug.value(), 16U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_17.value(), 17U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_18.value(), 18U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_19.value(), 19U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_20.value(), 20U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_21.value(), 21U);
    EXPECT_EQ(tpmkit::pcr_index::drtm_22.value(), 22U);
    EXPECT_EQ(tpmkit::pcr_index::application.value(), 23U);
}

TEST(pcr_index, compares_and_orders_by_numeric_value)
{
    // Verifies PCR indices compare by value and support ordered containers.

    const tpmkit::pcr_index first{1U};
    const tpmkit::pcr_index same_first{1U};
    const tpmkit::pcr_index second{2U};
    const std::set<tpmkit::pcr_index> ordered{second, first, same_first};

    EXPECT_EQ(first, same_first);
    EXPECT_NE(first, second);
    EXPECT_LT(first, second);
    ASSERT_EQ(ordered.size(), 2U);
    EXPECT_EQ(ordered.begin()->value(), 1U);
}

TEST(pcr_index, supports_hash_containers)
{
    // Verifies PCR indices can be used in unordered containers.

    std::unordered_set<tpmkit::pcr_index> indices;

    indices.insert(tpmkit::pcr_index::debug);
    indices.insert(tpmkit::pcr_index{16U});
    indices.insert(tpmkit::pcr_index::application);

    EXPECT_EQ(indices.size(), 2U);
    EXPECT_EQ(indices.count(tpmkit::pcr_index::debug), 1U);
}

TEST(pcr_bank, constructs_from_supported_hash_algorithm)
{
    // Verifies PCR banks preserve supported algorithms and digest sizes.

    const tpmkit::pcr_bank sha1{tpmkit::hash_algorithm::sha1};
    const tpmkit::pcr_bank sha256{tpmkit::hash_algorithm::sha256};
    const tpmkit::pcr_bank sha384{tpmkit::hash_algorithm::sha384};
    const tpmkit::pcr_bank sha512{tpmkit::hash_algorithm::sha512};

    EXPECT_EQ(sha1.algorithm(), tpmkit::hash_algorithm::sha1);
    EXPECT_EQ(sha1.digest_size(), 20U);
    EXPECT_EQ(sha256.digest_size(), 32U);
    EXPECT_EQ(sha384.digest_size(), 48U);
    EXPECT_EQ(sha512.digest_size(), 64U);
}

TEST(pcr_bank, rejects_unsupported_hash_algorithm)
{
    // Verifies PCR banks reject values outside the hash algorithm vocabulary.

    EXPECT_THROW(static_cast<void>(tpmkit::pcr_bank{static_cast<tpmkit::hash_algorithm>(99)}),
                 tpmkit::input_validation_error);
}

TEST(pcr_bank, compares_by_algorithm_and_digest_size)
{
    // Verifies PCR banks compare by value.

    const tpmkit::pcr_bank first{tpmkit::hash_algorithm::sha256};
    const tpmkit::pcr_bank same_first{tpmkit::hash_algorithm::sha256};
    const tpmkit::pcr_bank second{tpmkit::hash_algorithm::sha384};

    EXPECT_EQ(first, same_first);
    EXPECT_NE(first, second);
}

TEST(pcr_digest_value, constructs_when_digest_size_matches_algorithm)
{
    // Verifies PCR digest values accept correctly-sized digest bytes.

    const auto expected = digest_bytes(32U);

    const tpmkit::pcr_digest_value value{tpmkit::hash_algorithm::sha256, expected};

    EXPECT_EQ(value.algorithm(), tpmkit::hash_algorithm::sha256);
    EXPECT_EQ(value.digest(), expected);
}

TEST(pcr_digest_value, rejects_digest_size_mismatch)
{
    // Verifies PCR digest values reject bytes that do not match the algorithm size.

    EXPECT_THROW(static_cast<void>(
                     tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(31U)}),
                 tpmkit::input_validation_error);
    EXPECT_THROW(static_cast<void>(
                     tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(33U)}),
                 tpmkit::input_validation_error);
}

TEST(pcr_digest_value, rejects_unsupported_hash_algorithm)
{
    // Verifies PCR digest values validate the algorithm before accepting bytes.

    EXPECT_THROW(static_cast<void>(tpmkit::pcr_digest_value{static_cast<tpmkit::hash_algorithm>(99),
                                                            digest_bytes(32U)}),
                 tpmkit::input_validation_error);
}

TEST(pcr_digest_value, compares_by_algorithm_and_digest_bytes)
{
    // Verifies PCR digest values compare by algorithm and byte content.

    const tpmkit::pcr_digest_value first{tpmkit::hash_algorithm::sha256, digest_bytes(32U, 0x01U)};
    const tpmkit::pcr_digest_value same_first{tpmkit::hash_algorithm::sha256,
                                              digest_bytes(32U, 0x01U)};
    const tpmkit::pcr_digest_value different_bytes{tpmkit::hash_algorithm::sha256,
                                                   digest_bytes(32U, 0x02U)};
    const tpmkit::pcr_digest_value different_algorithm{tpmkit::hash_algorithm::sha384,
                                                       digest_bytes(48U, 0x01U)};

    EXPECT_EQ(first, same_first);
    EXPECT_NE(first, different_bytes);
    EXPECT_NE(first, different_algorithm);
}

TEST(pcr_selection, constructs_from_index_set)
{
    // Verifies PCR selections store a sorted unique set of indices for one bank.

    const std::set<tpmkit::pcr_index> indices{tpmkit::pcr_index::application,
                                              tpmkit::pcr_index::debug};

    const tpmkit::pcr_selection selection{tpmkit::hash_algorithm::sha256, indices};

    EXPECT_EQ(selection.algorithm(), tpmkit::hash_algorithm::sha256);
    EXPECT_EQ(selection.indices(), indices);
}

TEST(pcr_selection, constructs_from_initializer_list)
{
    // Verifies PCR selections support ergonomic initializer-list construction.

    const tpmkit::pcr_selection selection{
        tpmkit::hash_algorithm::sha384,
        {tpmkit::pcr_index::application, tpmkit::pcr_index::debug, tpmkit::pcr_index::debug},
    };

    EXPECT_EQ(selection.algorithm(), tpmkit::hash_algorithm::sha384);
    EXPECT_EQ(selection.indices().size(), 2U);
    EXPECT_EQ(selection.indices().count(tpmkit::pcr_index::debug), 1U);
}

TEST(pcr_selection, accepts_empty_selection)
{
    // Verifies an empty PCR selection is a valid value object.

    const tpmkit::pcr_selection selection{tpmkit::hash_algorithm::sha512};

    EXPECT_EQ(selection.algorithm(), tpmkit::hash_algorithm::sha512);
    EXPECT_TRUE(selection.indices().empty());
}

TEST(pcr_selection, rejects_unsupported_hash_algorithm)
{
    // Verifies PCR selections validate the selected bank algorithm.

    EXPECT_THROW(static_cast<void>(tpmkit::pcr_selection{static_cast<tpmkit::hash_algorithm>(99),
                                                         {tpmkit::pcr_index::debug}}),
                 tpmkit::input_validation_error);
}

TEST(pcr_selection, compares_by_algorithm_and_indices)
{
    // Verifies PCR selections compare by bank algorithm and selected indices.

    const tpmkit::pcr_selection first{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}};
    const tpmkit::pcr_selection same_first{tpmkit::hash_algorithm::sha256,
                                           {tpmkit::pcr_index::debug}};
    const tpmkit::pcr_selection different_algorithm{tpmkit::hash_algorithm::sha384,
                                                    {tpmkit::pcr_index::debug}};
    const tpmkit::pcr_selection different_indices{tpmkit::hash_algorithm::sha256,
                                                  {tpmkit::pcr_index::application}};

    EXPECT_EQ(first, same_first);
    EXPECT_NE(first, different_algorithm);
    EXPECT_NE(first, different_indices);
}

TEST(pcr_result_types, pcr_read_result_holds_values_and_compares_by_value)
{
    // Verifies PCR read results hold the counter, values, and actual selection.

    const tpmkit::pcr_read_result result{
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}},
        42U,
        std::vector<tpmkit::pcr_digest_value>{
            tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(32U, 0x11U)},
        },
    };
    const tpmkit::pcr_read_result same_result{
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}},
        42U,
        std::vector<tpmkit::pcr_digest_value>{
            tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(32U, 0x11U)},
        },
    };

    EXPECT_EQ(result.update_counter, 42U);
    EXPECT_EQ(result.values.size(), 1U);
    EXPECT_EQ(result.actual_selection.indices().count(tpmkit::pcr_index::debug), 1U);
    EXPECT_EQ(result, same_result);
}

TEST(pcr_result_types, pcr_event_result_holds_digests_and_compares_by_value)
{
    // Verifies PCR event results hold resulting digests.

    const tpmkit::pcr_event_result result{
        std::vector<tpmkit::pcr_digest_value>{
            tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(32U, 0x21U)},
            tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha384, digest_bytes(48U, 0x22U)},
        },
    };
    const tpmkit::pcr_event_result different{
        std::vector<tpmkit::pcr_digest_value>{
            tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(32U, 0x23U)},
        },
    };

    EXPECT_EQ(result.digests.size(), 2U);
    EXPECT_NE(result, different);
}

TEST(pcr_result_types, pcr_allocate_result_holds_capacity_fields_and_compares_by_value)
{
    // Verifies PCR allocation results hold success and capacity fields.

    const tpmkit::pcr_allocate_result result{true, 24U, 96U, 128U};
    const tpmkit::pcr_allocate_result same_result{true, 24U, 96U, 128U};
    const tpmkit::pcr_allocate_result different{false, 24U, 96U, 128U};

    EXPECT_TRUE(result.allocation_success);
    EXPECT_EQ(result.max_pcr, 24U);
    EXPECT_EQ(result.size_needed, 96U);
    EXPECT_EQ(result.size_available, 128U);
    EXPECT_EQ(result, same_result);
    EXPECT_NE(result, different);
}

TEST(pcr_value_types, are_immutable_value_objects)
{
    // Verifies PCR value types expose value semantics without assignment-time mutation.

    EXPECT_TRUE(std::is_copy_constructible<tpmkit::pcr_index>::value);
    EXPECT_TRUE(std::is_copy_constructible<tpmkit::pcr_bank>::value);
    EXPECT_TRUE(std::is_copy_constructible<tpmkit::pcr_digest_value>::value);
    EXPECT_TRUE(std::is_copy_constructible<tpmkit::pcr_selection>::value);
    EXPECT_FALSE(
        (std::is_assignable<decltype(tpmkit::pcr_index::debug.value()), std::uint8_t>::value));
    EXPECT_TRUE((std::is_constructible<std::vector<tpmkit::pcr_digest_value>,
                                       std::initializer_list<tpmkit::pcr_digest_value>>::value));
}

} // namespace
