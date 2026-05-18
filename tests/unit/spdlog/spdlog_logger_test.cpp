#include <tpmkit/spdlog_logger.h>

#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <sstream>
#include <string>

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

} // namespace
