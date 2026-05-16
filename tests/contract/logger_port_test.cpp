#include <tpmkit/noop_logger.h>
#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

void log_through_port(tpmkit::logger& log)
{
    log.log(tpmkit::log_level::info, "through port", gsl::span<const tpmkit::log_field>{});
}

} // namespace

TEST(logger_port, noop_logger_is_noop_and_does_not_throw)
{
    tpmkit::noop_logger log;
    const std::array<tpmkit::log_field, 1> fields{{{"key", "value"}}};

    EXPECT_NO_THROW(log.log(tpmkit::log_level::trace, "ignored", gsl::span<const tpmkit::log_field>{fields}));
    EXPECT_NO_THROW(log_through_port(log));
}

TEST(logger_port, recording_logger_constructs_with_empty_snapshot)
{
    tpmkit::testing::recording_logger log;

    EXPECT_TRUE(log.snapshot().empty());
}

TEST(logger_port, recording_logger_captures_level_and_message)
{
    tpmkit::testing::recording_logger log;

    log.log(tpmkit::log_level::info, "x", gsl::span<const tpmkit::log_field>{});

    const std::vector<tpmkit::testing::log_record> records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, tpmkit::log_level::info);
    EXPECT_EQ(records.front().message, "x");
    EXPECT_TRUE(records.front().fields.empty());
}

TEST(logger_port, recording_logger_captures_owned_fields)
{
    tpmkit::testing::recording_logger log;
    const std::array<tpmkit::log_field, 1> fields{{{"a", "1"}}};

    log.log(tpmkit::log_level::error, "y", gsl::span<const tpmkit::log_field>{fields});

    const std::vector<tpmkit::testing::log_record> records = log.snapshot();
    ASSERT_EQ(records.size(), 1U);
    ASSERT_EQ(records.front().fields.size(), 1U);
    EXPECT_EQ(records.front().fields.front().first, "a");
    EXPECT_EQ(records.front().fields.front().second, "1");
}

TEST(logger_port, recording_logger_does_not_retain_caller_buffers)
{
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

TEST(logger_port, recording_logger_clear_empties_snapshot)
{
    tpmkit::testing::recording_logger log;

    log.log(tpmkit::log_level::info, "x", gsl::span<const tpmkit::log_field>{});
    log.clear();

    EXPECT_TRUE(log.snapshot().empty());
}

TEST(logger_port, recording_logger_is_thread_safe_for_concurrent_writers)
{
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

            for (std::size_t record_index = thread_index; record_index < total_records; record_index += thread_count) {
                log.log(tpmkit::log_level::debug, std::to_string(record_index), gsl::span<const tpmkit::log_field>{});
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

TEST(logger_port, loggers_are_assignable_to_shared_ptr_and_reference_port_shapes)
{
    std::shared_ptr<tpmkit::logger> noop = std::make_shared<tpmkit::noop_logger>();
    std::shared_ptr<tpmkit::logger> recording = std::make_shared<tpmkit::testing::recording_logger>();

    ASSERT_NE(noop, nullptr);
    ASSERT_NE(recording, nullptr);
    EXPECT_NO_THROW(log_through_port(*noop));
    EXPECT_NO_THROW(log_through_port(*recording));
}
