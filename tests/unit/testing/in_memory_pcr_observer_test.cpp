#include <tpmkit/testing/in_memory_pcr_observer.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] tpmkit::pcr_digest_value make_sha256_digest(const std::uint8_t fill)
{
    return tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256,
                                    std::vector<std::uint8_t>(32U, fill)};
}

static_assert(noexcept(std::declval<tpmkit::testing::in_memory_pcr_observer&>().on_extend(
                  std::declval<tpmkit::pcr_index>(),
                  std::declval<gsl::span<const tpmkit::pcr_digest_value>>())),
              "in_memory_pcr_observer::on_extend must be noexcept");

static_assert(noexcept(std::declval<tpmkit::testing::in_memory_pcr_observer&>().on_event(
                  std::declval<tpmkit::pcr_index>(), std::declval<gsl::span<const std::uint8_t>>(),
                  std::declval<const tpmkit::pcr_event_result&>())),
              "in_memory_pcr_observer::on_event must be noexcept");

} // namespace

TEST(in_memory_pcr_observer, on_extend_records_correct_index_and_digests)
{
    // Verifies extend notifications are copied into a measurement record.

    tpmkit::testing::in_memory_pcr_observer observer;
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x11U)}};

    observer.on_extend(tpmkit::pcr_index::debug,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});

    ASSERT_EQ(observer.count(), 1U);
    const tpmkit::testing::pcr_measurement_record& record = observer.entries().front();
    const std::vector<tpmkit::pcr_digest_value> expected_digests{digests.begin(), digests.end()};
    EXPECT_EQ(record.index, tpmkit::pcr_index::debug);
    EXPECT_EQ(record.operation, tpmkit::testing::pcr_measurement_operation::extend);
    EXPECT_EQ(record.digests, expected_digests);
    EXPECT_TRUE(record.event_data.empty());
}

TEST(in_memory_pcr_observer, on_event_records_correct_index_event_data_and_result)
{
    // Verifies event notifications copy event bytes and result digests.

    tpmkit::testing::in_memory_pcr_observer observer;
    const std::array<std::uint8_t, 3> event_data{{0x01U, 0x02U, 0x03U}};
    const tpmkit::pcr_event_result result{{make_sha256_digest(0x22U)}};

    observer.on_event(tpmkit::pcr_index::application, gsl::span<const std::uint8_t>{event_data},
                      result);

    ASSERT_EQ(observer.count(), 1U);
    const tpmkit::testing::pcr_measurement_record& record = observer.entries().front();
    const std::vector<std::uint8_t> expected_event_data{event_data.begin(), event_data.end()};
    EXPECT_EQ(record.index, tpmkit::pcr_index::application);
    EXPECT_EQ(record.operation, tpmkit::testing::pcr_measurement_operation::event);
    EXPECT_EQ(record.event_data, expected_event_data);
    EXPECT_EQ(record.digests, result.digests);
}

TEST(in_memory_pcr_observer, entries_returns_all_recorded_measurements_in_order)
{
    // Verifies entries preserves the order in which notifications arrived.

    tpmkit::testing::in_memory_pcr_observer observer;
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x33U)}};
    const tpmkit::pcr_event_result result{{make_sha256_digest(0x44U)}};

    observer.on_extend(tpmkit::pcr_index::debug,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});
    observer.on_event(tpmkit::pcr_index::application, gsl::span<const std::uint8_t>{}, result);

    ASSERT_EQ(observer.entries().size(), 2U);
    EXPECT_EQ(observer.entries()[0].operation, tpmkit::testing::pcr_measurement_operation::extend);
    EXPECT_EQ(observer.entries()[1].operation, tpmkit::testing::pcr_measurement_operation::event);
    EXPECT_EQ(observer.entries()[0].index, tpmkit::pcr_index::debug);
    EXPECT_EQ(observer.entries()[1].index, tpmkit::pcr_index::application);
}

TEST(in_memory_pcr_observer, entries_by_index_returns_matching_records)
{
    // Verifies index filtering returns only records for the requested PCR.

    tpmkit::testing::in_memory_pcr_observer observer;
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x55U)}};

    observer.on_extend(tpmkit::pcr_index::debug,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});
    observer.on_extend(tpmkit::pcr_index::application,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});
    observer.on_extend(tpmkit::pcr_index::debug,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});

    const std::vector<tpmkit::testing::pcr_measurement_record> records =
        observer.entries_by_index(tpmkit::pcr_index::debug);

    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(records[0].index, tpmkit::pcr_index::debug);
    EXPECT_EQ(records[1].index, tpmkit::pcr_index::debug);
}

TEST(in_memory_pcr_observer, clear_resets_recording)
{
    // Verifies clear removes every captured measurement.

    tpmkit::testing::in_memory_pcr_observer observer;
    const std::array<tpmkit::pcr_digest_value, 1> digests{{make_sha256_digest(0x66U)}};

    observer.on_extend(tpmkit::pcr_index::debug,
                       gsl::span<const tpmkit::pcr_digest_value>{digests});
    observer.clear();

    EXPECT_EQ(observer.count(), 0U);
    EXPECT_TRUE(observer.entries().empty());
}

TEST(in_memory_pcr_observer, callbacks_are_noexcept)
{
    // Verifies observer callbacks satisfy the compile-time noexcept contract.

    EXPECT_TRUE(std::is_nothrow_destructible<tpmkit::testing::in_memory_pcr_observer>::value);
}
