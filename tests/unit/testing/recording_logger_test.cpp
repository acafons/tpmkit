#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace {

TEST(recording_logger, constructs_with_empty_snapshot)
{
    // Verifies a new recording logger starts with no captured records.

    tpmkit::testing::recording_logger log;

    EXPECT_TRUE(log.snapshot().empty());
}

TEST(recording_logger, captures_level_and_message)
{
    // Verifies recording logger copies the level and message from a log call.

    tpmkit::testing::recording_logger log;

    log.log(tpmkit::log_level::info, "x", gsl::span<const tpmkit::log_field>{});

    const std::vector<tpmkit::testing::log_record> records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, tpmkit::log_level::info);
    EXPECT_EQ(records.front().message, "x");
    EXPECT_TRUE(records.front().fields.empty());
}

TEST(recording_logger, captures_owned_fields)
{
    // Verifies recording logger owns copied field key/value pairs.

    tpmkit::testing::recording_logger log;
    const std::array<tpmkit::log_field, 1> fields{{{"a", "1"}}};

    log.log(tpmkit::log_level::error, "y", gsl::span<const tpmkit::log_field>{fields});

    const std::vector<tpmkit::testing::log_record> records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    ASSERT_EQ(records.front().fields.size(), 1U);
    EXPECT_EQ(records.front().fields.front().first, "a");
    EXPECT_EQ(records.front().fields.front().second, "1");
}

TEST(recording_logger, does_not_retain_caller_buffers)
{
    // Verifies captured records remain stable after caller buffers change.

    tpmkit::testing::recording_logger log;
    std::string message = "temporary message";
    std::string key = "field_key";
    std::string value = "field_value";
    std::array<tpmkit::log_field, 1> fields{{{key, value}}};

    log.log(tpmkit::log_level::warn, message, gsl::span<const tpmkit::log_field>{fields});
    std::fill(message.begin(), message.end(), '#');
    std::fill(key.begin(), key.end(), '#');
    std::fill(value.begin(), value.end(), '#');

    const std::vector<tpmkit::testing::log_record> records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    ASSERT_EQ(records.front().fields.size(), 1U);
    EXPECT_EQ(records.front().message, "temporary message");
    EXPECT_EQ(records.front().fields.front().first, "field_key");
    EXPECT_EQ(records.front().fields.front().second, "field_value");
}

TEST(recording_logger, clear_empties_snapshot)
{
    // Verifies clear removes every captured record.

    tpmkit::testing::recording_logger log;

    log.log(tpmkit::log_level::info, "x", gsl::span<const tpmkit::log_field>{});
    log.clear();

    EXPECT_TRUE(log.snapshot().empty());
}

TEST(recording_logger, is_thread_safe_for_concurrent_writers)
{
    // Verifies concurrent writers can append records without losing entries.

    constexpr std::size_t thread_count = 8U;
    constexpr std::size_t total_records = 100U;
    tpmkit::testing::recording_logger log;
    std::atomic<std::size_t> ready_count{0U};
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t thread_index = 0U; thread_index < thread_count; ++thread_index) {
        threads.emplace_back([&, thread_index] {
            ready_count.fetch_add(1U, std::memory_order_acq_rel);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (std::size_t record_index = thread_index; record_index < total_records;
                 record_index += thread_count) {
                log.log(tpmkit::log_level::debug, std::to_string(record_index),
                        gsl::span<const tpmkit::log_field>{});
            }
        });
    }

    while (ready_count.load(std::memory_order_acquire) != thread_count) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(log.snapshot().size(), total_records);
}

} // namespace
