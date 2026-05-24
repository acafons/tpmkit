#include <tpmkit/logging/spdlog_logger.h>

#include <tpmkit/exception.h>

#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// Build a spdlog_logger that writes to an in-memory ostringstream.
std::pair<tpmkit::spdlog_logger, std::shared_ptr<std::ostringstream>> make_test_logger(
    const ::spdlog::level::level_enum min_level = ::spdlog::level::trace)
{
    auto stream = std::make_shared<std::ostringstream>();
    auto sink = std::make_shared<::spdlog::sinks::ostream_sink_mt>(*stream);
    sink->set_level(min_level);
    sink->set_pattern("%v");  // message only — keeps assertions simple
    auto inner = std::make_shared<::spdlog::logger>("tpmkit", sink);
    inner->set_level(min_level);
    return {tpmkit::spdlog_logger{inner}, stream};
}

class detecting_single_thread_sink final : public ::spdlog::sinks::base_sink<::spdlog::details::null_mutex> {
public:
    [[nodiscard]] std::size_t records_observed() const noexcept
    {
        return records_observed_.load();
    }

    [[nodiscard]] bool saw_overlap() const noexcept
    {
        return saw_overlap_.load();
    }

protected:
    void sink_it_(const ::spdlog::details::log_msg&) final
    {
        if (active_writers_.fetch_add(1) != 0) {
            saw_overlap_.store(true);
        }

        for (int iteration = 0; iteration < 1000; ++iteration) {
            std::this_thread::yield();
        }

        records_observed_.fetch_add(1);
        active_writers_.fetch_sub(1);
    }

    void flush_() final {}

private:
    std::atomic<int> active_writers_{0};
    std::atomic<std::size_t> records_observed_{0U};
    std::atomic<bool> saw_overlap_{false};
};

TEST(spdlog_logger, rejects_null_sink)
{
    // Verifies null wrapped spdlog loggers are rejected at construction.

    EXPECT_THROW(
        tpmkit::spdlog_logger{std::shared_ptr<::spdlog::logger>{}},
        tpmkit::tpmkit_error);
}

TEST(spdlog_logger, forwards_message_to_sink)
{
    // Verifies the message text appears verbatim in the rendered output.

    auto [log, stream] = make_test_logger();

    log.log(tpmkit::log_level::info, "tpm.context.tcti_configured",
            gsl::span<const tpmkit::log_field>{});
    log.flush();

    EXPECT_NE(stream->str().find("tpm.context.tcti_configured"), std::string::npos);
}

TEST(spdlog_logger, appends_fields_as_key_value_pairs)
{
    // Verifies each field is rendered as key=value after the message.

    auto [log, stream] = make_test_logger();
    const std::array<tpmkit::log_field, 2> fields{{
        {"event",   "tpm.session_open"},
        {"outcome", "success"},
    }};

    log.log(tpmkit::log_level::info, "session opened",
            gsl::span<const tpmkit::log_field>{fields});
    log.flush();

    const std::string output = stream->str();
    EXPECT_NE(output.find("event=tpm.session_open"), std::string::npos);
    EXPECT_NE(output.find("outcome=success"), std::string::npos);
}

TEST(spdlog_logger, quotes_and_escapes_values_with_whitespace)
{
    // Verifies values containing spaces are double-quoted in the output.

    auto [log, stream] = make_test_logger();
    const std::array<tpmkit::log_field, 1> fields{{
        {"message", "hello world"},
    }};

    log.log(tpmkit::log_level::warn, "test",
            gsl::span<const tpmkit::log_field>{fields});
    log.flush();

    EXPECT_NE(stream->str().find("message=\"hello world\""), std::string::npos);
}

TEST(spdlog_logger, escapes_newline_in_field_value)
{
    // Verifies embedded newlines in field values are escaped to prevent log injection.

    auto [log, stream] = make_test_logger();
    const std::array<tpmkit::log_field, 1> fields{{
        {"reason", "line1\nline2"},
    }};

    log.log(tpmkit::log_level::error, "test",
            gsl::span<const tpmkit::log_field>{fields});
    log.flush();

    EXPECT_NE(stream->str().find("reason=\"line1\\nline2\""), std::string::npos);
}

TEST(spdlog_logger, suppresses_records_below_active_level)
{
    // Verifies records below the sink's active level are not forwarded.

    auto [log, stream] = make_test_logger(::spdlog::level::warn);

    log.log(tpmkit::log_level::debug, "should not appear",
            gsl::span<const tpmkit::log_field>{});
    log.flush();

    EXPECT_TRUE(stream->str().empty());
}

TEST(spdlog_logger, flush_does_not_throw)
{
    // Verifies flush() is callable without error on a live logger.

    auto [log, stream] = make_test_logger();

    EXPECT_NO_THROW(log.flush());
}

TEST(spdlog_logger, log_with_nullptr_fields_does_not_crash)
{
    // Verifies an empty field span is handled safely.

    auto [log, stream] = make_test_logger();

    EXPECT_NO_THROW(log.log(tpmkit::log_level::info, "tpm.context.finalized",
                            gsl::span<const tpmkit::log_field>{}));
    log.flush();
    EXPECT_NE(stream->str().find("tpm.context.finalized"), std::string::npos);
}

TEST(spdlog_logger, serializes_calls_to_single_threaded_sink)
{
    // Verifies adapter-mediated calls are serialized for single-threaded sinks.

    auto sink = std::make_shared<detecting_single_thread_sink>();
    sink->set_level(::spdlog::level::trace);
    auto inner = std::make_shared<::spdlog::logger>("single_threaded_sink", sink);
    inner->set_level(::spdlog::level::trace);
    tpmkit::spdlog_logger log{inner};

    constexpr int thread_count = 8;
    constexpr int records_per_thread = 50;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([&log] {
            for (int record = 0; record < records_per_thread; ++record) {
                log.log(tpmkit::log_level::info, "tpm.context.finalized",
                        gsl::span<const tpmkit::log_field>{});
            }
        });
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(sink->records_observed(),
              static_cast<std::size_t>(thread_count * records_per_thread));
    EXPECT_FALSE(sink->saw_overlap());
}

} // namespace
