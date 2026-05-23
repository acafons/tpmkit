#include <tpmkit/logging/stdio_logger.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <future>
#include <ios>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

static_assert(noexcept(tpmkit::stdio_logger{}),
              "stdio_logger default construction must be noexcept");
static_assert(std::is_nothrow_destructible<tpmkit::stdio_logger>::value,
              "stdio_logger destruction must be noexcept");
static_assert(std::is_nothrow_move_constructible<tpmkit::stdio_logger>::value,
              "stdio_logger move construction must be noexcept");
static_assert(std::is_nothrow_move_assignable<tpmkit::stdio_logger>::value,
              "stdio_logger move assignment must be noexcept");
static_assert(!std::is_copy_constructible<tpmkit::stdio_logger>::value,
              "stdio_logger must be move-only");
static_assert(!std::is_copy_assignable<tpmkit::stdio_logger>::value,
              "stdio_logger must be move-only");
static_assert(
    noexcept(std::declval<tpmkit::stdio_logger&>().log(tpmkit::log_level::info, std::string_view{},
                                                       gsl::span<const tpmkit::log_field>{})),
    "stdio_logger::log must be noexcept");

constexpr std::string_view ansi_reset = "\x1b[0m";

class env_var_guard {
public:
    explicit env_var_guard(const char* name) : name_{name}
    {
        const char* const existing = std::getenv(name_);
        if (existing != nullptr) {
            had_value_ = true;
            value_ = existing;
        }
    }

    ~env_var_guard()
    {
        if (had_value_) {
            static_cast<void>(::setenv(name_, value_.c_str(), 1));
        } else {
            static_cast<void>(::unsetenv(name_));
        }
    }

    env_var_guard(const env_var_guard&) = delete;
    env_var_guard& operator=(const env_var_guard&) = delete;

    void set(const char* const value)
    {
        static_cast<void>(::setenv(name_, value, 1));
    }

    void unset()
    {
        static_cast<void>(::unsetenv(name_));
    }

private:
    bool had_value_{false};
    const char* name_;
    std::string value_;
};

class streambuf_guard {
public:
    streambuf_guard(std::ostream& stream, std::streambuf* replacement)
        : original_{stream.rdbuf(replacement)}, stream_{stream}
    {}

    ~streambuf_guard()
    {
        stream_.rdbuf(original_);
    }

    streambuf_guard(const streambuf_guard&) = delete;
    streambuf_guard& operator=(const streambuf_guard&) = delete;

private:
    std::streambuf* original_;
    std::ostream& stream_;
};

const std::regex& line_regex_with_newline()
{
    static const std::regex pattern{
        "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z "
        "\\[(TRACE|DEBUG|INFO |WARN |ERROR)\\] .*\\n$"};
    return pattern;
}

const std::regex& line_regex_without_newline()
{
    static const std::regex pattern{
        "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}Z "
        "\\[(TRACE|DEBUG|INFO |WARN |ERROR)\\] .*$"};
    return pattern;
}

tpmkit::stdio_logger make_logger(std::ostringstream& out, std::ostringstream& err,
                                 const tpmkit::color_mode color = tpmkit::color_mode::never,
                                 const std::optional<tpmkit::log_level> min_level = std::nullopt)
{
    tpmkit::stdio_logger_options options;
    options.min_level = min_level;
    options.color = color;
    options.out = &out;
    options.err = &err;
    return tpmkit::stdio_logger{options};
}

std::string output_for(const tpmkit::log_level level,
                       const tpmkit::color_mode color = tpmkit::color_mode::never)
{
    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err, color);

    log.log(level, "msg", gsl::span<const tpmkit::log_field>{});

    return level == tpmkit::log_level::warn || level == tpmkit::log_level::error ? err.str()
                                                                                 : out.str();
}

std::string_view expected_ansi_sequence(const tpmkit::log_level level)
{
    switch (level) {
    case tpmkit::log_level::trace:
        return "\x1b[2m";
    case tpmkit::log_level::debug:
        return "\x1b[36m";
    case tpmkit::log_level::info:
        return "\x1b[32m";
    case tpmkit::log_level::warn:
        return "\x1b[33m";
    case tpmkit::log_level::error:
        return "\x1b[31m";
    }
    return "";
}

std::string_view expected_label(const tpmkit::log_level level)
{
    switch (level) {
    case tpmkit::log_level::trace:
        return "TRACE";
    case tpmkit::log_level::debug:
        return "DEBUG";
    case tpmkit::log_level::info:
        return "INFO ";
    case tpmkit::log_level::warn:
        return "WARN ";
    case tpmkit::log_level::error:
        return "ERROR";
    }
    return "INFO ";
}

bool contains_ansi_escape(const std::string& output)
{
    return output.find("\x1b[") != std::string::npos;
}

void expect_routes_to_err(const tpmkit::log_level level)
{
    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);

    log.log(level, "msg", gsl::span<const tpmkit::log_field>{});

    EXPECT_TRUE(out.str().empty());
    EXPECT_NE(err.str().find("msg"), std::string::npos);
}

void expect_routes_to_out(const tpmkit::log_level level)
{
    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);

    log.log(level, "msg", gsl::span<const tpmkit::log_field>{});

    EXPECT_NE(out.str().find("msg"), std::string::npos);
    EXPECT_TRUE(err.str().empty());
}

std::string level_token_from_line(const std::string& line)
{
    const std::size_t open = line.find('[');
    const std::size_t close = line.find(']', open);
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return {};
    }
    return line.substr(open + 1U, close - open - 1U);
}

TEST(stdio_logger, routes_info_to_out)
{
    // Verifies info records are written to the substituted output stream.

    expect_routes_to_out(tpmkit::log_level::info);
}

TEST(stdio_logger, routes_debug_to_out)
{
    // Verifies debug records are written to the substituted output stream.

    expect_routes_to_out(tpmkit::log_level::debug);
}

TEST(stdio_logger, routes_trace_to_out)
{
    // Verifies trace records are written to the substituted output stream.

    expect_routes_to_out(tpmkit::log_level::trace);
}

TEST(stdio_logger, routes_warn_to_err)
{
    // Verifies warn records are written to the substituted error stream.

    expect_routes_to_err(tpmkit::log_level::warn);
}

TEST(stdio_logger, routes_error_to_err)
{
    // Verifies error records are written to the substituted error stream.

    expect_routes_to_err(tpmkit::log_level::error);
}

TEST(stdio_logger, renders_expected_line_format)
{
    // Verifies records use UTC millisecond timestamps and fixed-width level tokens.

    const std::string output = output_for(tpmkit::log_level::info);

    EXPECT_TRUE(std::regex_match(output, line_regex_with_newline())) << output;
}

TEST(stdio_logger, pads_each_level_token_to_five_characters)
{
    // Verifies every bracketed level token is exactly five characters wide.

    const std::array<tpmkit::log_level, 5> levels{{
        tpmkit::log_level::trace,
        tpmkit::log_level::debug,
        tpmkit::log_level::info,
        tpmkit::log_level::warn,
        tpmkit::log_level::error,
    }};

    for (const tpmkit::log_level level : levels) {
        const std::string token = level_token_from_line(output_for(level));
        EXPECT_EQ(token.size(), 5U);
        EXPECT_EQ(token, expected_label(level));
    }
}

TEST(stdio_logger, renders_plain_ascii_field_unquoted)
{
    // Verifies plain field values round-trip as unquoted key=value pairs.

    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);
    const std::array<tpmkit::log_field, 1> fields{{{"key", "value"}}};

    log.log(tpmkit::log_level::info, "msg", gsl::span<const tpmkit::log_field>{fields});

    EXPECT_NE(out.str().find("key=value"), std::string::npos);
}

TEST(stdio_logger, quotes_field_value_containing_space)
{
    // Verifies field values containing spaces are quoted.

    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);
    const std::array<tpmkit::log_field, 1> fields{{{"key", "hello world"}}};

    log.log(tpmkit::log_level::info, "msg", gsl::span<const tpmkit::log_field>{fields});

    EXPECT_NE(out.str().find("key=\"hello world\""), std::string::npos);
}

TEST(stdio_logger, escapes_field_value_control_characters_end_to_end)
{
    // Verifies the shared field escaper is used by the stdio adapter.

    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);
    const std::array<tpmkit::log_field, 1> fields{{{"reason", "line1\nline2"}}};

    log.log(tpmkit::log_level::info, "msg", gsl::span<const tpmkit::log_field>{fields});

    EXPECT_NE(out.str().find("reason=\"line1\\nline2\""), std::string::npos);
}

TEST(stdio_logger, suppresses_records_below_min_level)
{
    // Verifies runtime minimum-level filtering drops lower-severity records.

    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err, tpmkit::color_mode::never, tpmkit::log_level::info);

    log.log(tpmkit::log_level::debug, "hidden", gsl::span<const tpmkit::log_field>{});

    EXPECT_TRUE(out.str().empty());
    EXPECT_TRUE(err.str().empty());
}

TEST(stdio_logger, never_color_mode_ignores_environment)
{
    // Verifies color_mode::never emits plain output even when color is forced.

    env_var_guard no_color{"NO_COLOR"};
    env_var_guard force_color{"FORCE_COLOR"};
    no_color.set("1");
    force_color.set("1");

    const std::string output = output_for(tpmkit::log_level::error, tpmkit::color_mode::never);

    EXPECT_FALSE(contains_ansi_escape(output));
}

TEST(stdio_logger, always_color_mode_wraps_only_level_token_and_ignores_environment)
{
    // Verifies color_mode::always emits the fixed palette around each level token.

    env_var_guard no_color{"NO_COLOR"};
    env_var_guard force_color{"FORCE_COLOR"};
    no_color.set("1");
    force_color.set("1");
    const std::array<tpmkit::log_level, 5> levels{{
        tpmkit::log_level::trace,
        tpmkit::log_level::debug,
        tpmkit::log_level::info,
        tpmkit::log_level::warn,
        tpmkit::log_level::error,
    }};

    for (const tpmkit::log_level level : levels) {
        const std::string output = output_for(level, tpmkit::color_mode::always);
        std::string expected;
        expected += expected_ansi_sequence(level);
        expected += '[';
        expected += expected_label(level);
        expected += ']';
        expected += ansi_reset;
        EXPECT_NE(output.find(expected), std::string::npos) << output;
    }
}

TEST(stdio_logger, auto_color_mode_keeps_non_tty_stream_plain)
{
    // Verifies automatic color falls back to plain output for substituted streams.

    env_var_guard no_color{"NO_COLOR"};
    env_var_guard force_color{"FORCE_COLOR"};
    no_color.unset();
    force_color.unset();

    const std::string output = output_for(tpmkit::log_level::info, tpmkit::color_mode::auto_);

    EXPECT_FALSE(contains_ansi_escape(output));
}

TEST(stdio_logger, auto_color_mode_honors_no_color_over_force_color)
{
    // Verifies NO_COLOR disables automatic color even when FORCE_COLOR is set.

    env_var_guard no_color{"NO_COLOR"};
    env_var_guard force_color{"FORCE_COLOR"};
    no_color.set("1");
    force_color.set("1");

    const std::string output = output_for(tpmkit::log_level::warn, tpmkit::color_mode::auto_);

    EXPECT_FALSE(contains_ansi_escape(output));
}

TEST(stdio_logger, auto_color_mode_honors_force_color_for_substituted_streams)
{
    // Verifies FORCE_COLOR enables automatic color for non-TTY substituted streams.

    env_var_guard no_color{"NO_COLOR"};
    env_var_guard force_color{"FORCE_COLOR"};
    no_color.unset();
    force_color.set("1");

    const std::string output = output_for(tpmkit::log_level::debug, tpmkit::color_mode::auto_);

    EXPECT_TRUE(contains_ansi_escape(output));
}

TEST(stdio_logger, substituted_streams_do_not_write_to_globals)
{
    // Verifies stream overrides isolate records from std::cout and std::cerr.

    std::ostringstream real_out;
    std::ostringstream real_err;
    streambuf_guard out_guard{std::cout, real_out.rdbuf()};
    streambuf_guard err_guard{std::cerr, real_err.rdbuf()};
    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);

    log.log(tpmkit::log_level::info, "out record", gsl::span<const tpmkit::log_field>{});
    log.log(tpmkit::log_level::warn, "err record", gsl::span<const tpmkit::log_field>{});

    EXPECT_NE(out.str().find("out record"), std::string::npos);
    EXPECT_NE(err.str().find("err record"), std::string::npos);
    EXPECT_TRUE(real_out.str().empty());
    EXPECT_TRUE(real_err.str().empty());
}

TEST(stdio_logger, serializes_concurrent_writers_into_complete_lines)
{
    // Verifies concurrent records are serialized as intact lines without interleaving.

    std::ostringstream out;
    std::ostringstream err;
    auto log = make_logger(out, err);
    constexpr int thread_count = 4;
    constexpr int records_per_thread = 1000;
    std::promise<void> start;
    std::shared_future<void> ready = start.get_future().share();
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int thread = 0; thread < thread_count; ++thread) {
        threads.emplace_back([&log, ready] {
            ready.wait();
            for (int record = 0; record < records_per_thread; ++record) {
                log.log(tpmkit::log_level::info, "threaded", gsl::span<const tpmkit::log_field>{});
            }
        });
    }

    start.set_value();
    for (std::thread& thread : threads) {
        thread.join();
    }

    const std::string output = out.str();
    EXPECT_TRUE(err.str().empty());
    ASSERT_FALSE(output.empty());
    EXPECT_EQ(output.back(), '\n');

    std::istringstream lines{output};
    std::string line;
    std::size_t count = 0U;
    while (std::getline(lines, line)) {
        ++count;
        EXPECT_TRUE(std::regex_match(line, line_regex_without_newline())) << line;
    }
    EXPECT_EQ(count, static_cast<std::size_t>(thread_count * records_per_thread));
}

TEST(stdio_logger, log_does_not_throw_when_stream_is_bad)
{
    // Verifies the noexcept logging contract with a bad substituted stream.

    std::ostringstream out;
    out.setstate(std::ios::badbit);
    std::ostringstream err;
    auto log = make_logger(out, err);

    EXPECT_NO_THROW(log.log(tpmkit::log_level::info, "msg", gsl::span<const tpmkit::log_field>{}));
}

TEST(stdio_logger, default_constructor_routes_to_cout_and_cerr)
{
    // Verifies default construction binds records to the platform global streams.

    std::ostringstream captured_out;
    std::ostringstream captured_err;
    streambuf_guard out_guard{std::cout, captured_out.rdbuf()};
    streambuf_guard err_guard{std::cerr, captured_err.rdbuf()};
    tpmkit::stdio_logger log;

    log.log(tpmkit::log_level::info, "default out", gsl::span<const tpmkit::log_field>{});
    log.log(tpmkit::log_level::error, "default err", gsl::span<const tpmkit::log_field>{});

    EXPECT_NE(captured_out.str().find("default out"), std::string::npos);
    EXPECT_NE(captured_err.str().find("default err"), std::string::npos);
}

TEST(stdio_logger, move_operations_transfer_streams_and_leave_source_noop)
{
    // Verifies moved-from loggers remain valid no-op objects and moved-to loggers write.

    std::ostringstream move_out;
    std::ostringstream move_err;
    auto source = make_logger(move_out, move_err);
    tpmkit::stdio_logger moved{std::move(source)};

    source.log(tpmkit::log_level::info, "moved from", gsl::span<const tpmkit::log_field>{});
    moved.log(tpmkit::log_level::info, "moved to", gsl::span<const tpmkit::log_field>{});

    EXPECT_EQ(move_out.str().find("moved from"), std::string::npos);
    EXPECT_NE(move_out.str().find("moved to"), std::string::npos);

    std::ostringstream assign_out;
    std::ostringstream assign_err;
    auto assign_source = make_logger(assign_out, assign_err);
    tpmkit::stdio_logger assigned;
    assigned = std::move(assign_source);

    assign_source.log(tpmkit::log_level::info, "assigned source",
                      gsl::span<const tpmkit::log_field>{});
    assigned.log(tpmkit::log_level::info, "assigned target", gsl::span<const tpmkit::log_field>{});

    EXPECT_EQ(assign_out.str().find("assigned source"), std::string::npos);
    EXPECT_NE(assign_out.str().find("assigned target"), std::string::npos);
}

} // namespace
