#include <tpmkit/logging/noop_logger.h>
#ifdef TPMKIT_HAS_SPDLOG_ADAPTER
#include <tpmkit/logging/spdlog_logger.h>
#endif
#ifdef TPMKIT_HAS_STDIO_ADAPTER
#include <tpmkit/logging/stdio_logger.h>
#endif
#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>
#ifdef TPMKIT_HAS_SPDLOG_ADAPTER
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#endif

#include <array>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct logger_under_test {
    std::vector<std::shared_ptr<void>> keep_alive;
    std::shared_ptr<tpmkit::logger> log;
    std::function<std::vector<std::string>()> records;
    bool emits_records;
};

struct logger_adapter_case {
    const char* name;
    std::function<logger_under_test()> create;
};

void PrintTo(const logger_adapter_case& test_case, std::ostream* const output)
{
    *output << test_case.name;
}

void log_through_port(tpmkit::logger& log)
{
    log.log(tpmkit::log_level::info, "through port", gsl::span<const tpmkit::log_field>{});
}

logger_under_test make_noop_case()
{
    return logger_under_test{
        {},
        std::make_shared<tpmkit::noop_logger>(),
        [] { return std::vector<std::string>{}; },
        false,
    };
}

logger_under_test make_recording_case()
{
    auto log = std::make_shared<tpmkit::testing::recording_logger>();
    return logger_under_test{
        {},
        log,
        [log] {
            std::vector<std::string> records;
            for (const tpmkit::testing::log_record& record : log->snapshot()) {
                std::string rendered{record.message};
                for (const auto& field : record.fields) {
                    rendered += ' ';
                    rendered += field.first;
                    rendered += '=';
                    rendered += field.second;
                }
                records.push_back(std::move(rendered));
            }
            return records;
        },
        true,
    };
}

#if defined(TPMKIT_HAS_SPDLOG_ADAPTER) || defined(TPMKIT_HAS_STDIO_ADAPTER)
std::vector<std::string> split_lines(const std::string& text)
{
    std::istringstream input{text};
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}
#endif

#ifdef TPMKIT_HAS_SPDLOG_ADAPTER
logger_under_test make_spdlog_case()
{
    auto stream = std::make_shared<std::ostringstream>();
    auto sink = std::make_shared<::spdlog::sinks::ostream_sink_mt>(*stream);
    sink->set_level(::spdlog::level::trace);
    sink->set_pattern("%v");
    auto inner = std::make_shared<::spdlog::logger>("tpmkit_contract", std::move(sink));
    inner->set_level(::spdlog::level::trace);

    return logger_under_test{
        std::vector<std::shared_ptr<void>>{stream},
        std::make_shared<tpmkit::spdlog_logger>(inner),
        [inner, stream] {
            inner->flush();
            return split_lines(stream->str());
        },
        true,
    };
}
#endif

#ifdef TPMKIT_HAS_STDIO_ADAPTER
logger_under_test make_stdio_case()
{
    auto out = std::make_shared<std::ostringstream>();
    auto err = std::make_shared<std::ostringstream>();
    tpmkit::stdio_logger_options options;
    options.color = tpmkit::color_mode::never;
    options.out = out.get();
    options.err = err.get();

    return logger_under_test{
        std::vector<std::shared_ptr<void>>{out, err},
        std::make_shared<tpmkit::stdio_logger>(options),
        [out, err] {
            std::vector<std::string> records = split_lines(out->str());
            std::vector<std::string> error_records = split_lines(err->str());
            records.insert(records.end(), std::make_move_iterator(error_records.begin()),
                           std::make_move_iterator(error_records.end()));
            return records;
        },
        true,
    };
}
#endif

class logger_port : public testing::TestWithParam<logger_adapter_case> {};

TEST_P(logger_port, accepts_log_calls_through_reference_shape)
{
    // Verifies each logger adapter accepts calls through the logger port.

    const logger_adapter_case& test_case = GetParam();
    logger_under_test subject = test_case.create();
    const std::array<tpmkit::log_field, 1> fields{{{"key", "value"}}};

    EXPECT_NO_THROW(subject.log->log(tpmkit::log_level::trace, "ignored",
                                     gsl::span<const tpmkit::log_field>{fields}));
    EXPECT_NO_THROW(log_through_port(*subject.log));
}

TEST_P(logger_port, is_assignable_to_shared_ptr_port_shape)
{
    // Verifies each logger adapter can be owned behind the logger port.

    const logger_adapter_case& test_case = GetParam();

    const logger_under_test subject = test_case.create();

    ASSERT_NE(subject.log, nullptr);
    EXPECT_NO_THROW(log_through_port(*subject.log));
}

TEST_P(logger_port, observable_adapters_write_one_complete_record)
{
    // Verifies adapters with observable sinks write exactly one complete record per call.

    const logger_adapter_case& test_case = GetParam();
    logger_under_test subject = test_case.create();
    const std::array<tpmkit::log_field, 1> fields{{{"key", "value"}}};

    subject.log->log(tpmkit::log_level::info, "complete record",
                     gsl::span<const tpmkit::log_field>{fields});

    const std::vector<std::string> records = subject.records();
    if (!subject.emits_records) {
        EXPECT_TRUE(records.empty());
        return;
    }

    ASSERT_EQ(records.size(), 1U);
    EXPECT_NE(records.front().find("complete record"), std::string::npos);
    EXPECT_NE(records.front().find("key=value"), std::string::npos);
}

TEST_P(logger_port, empty_field_span_does_not_render_stray_field_separator)
{
    // Verifies empty field lists do not render trailing spaces or stray key separators.

    const logger_adapter_case& test_case = GetParam();
    logger_under_test subject = test_case.create();

    subject.log->log(tpmkit::log_level::info, "empty fields", gsl::span<const tpmkit::log_field>{});

    const std::vector<std::string> records = subject.records();
    if (!subject.emits_records) {
        EXPECT_TRUE(records.empty());
        return;
    }

    ASSERT_EQ(records.size(), 1U);
    ASSERT_FALSE(records.front().empty());
    EXPECT_NE(records.front().find("empty fields"), std::string::npos);
    EXPECT_EQ(records.front().find('='), std::string::npos);
    EXPECT_NE(records.front().back(), ' ');
}

TEST_P(logger_port, concurrent_log_calls_are_safe)
{
    // Verifies concurrent calls through the shared logger port do not throw or lose records.

    constexpr std::size_t thread_count = 8U;
    const logger_adapter_case& test_case = GetParam();
    logger_under_test subject = test_case.create();
    std::promise<void> start_promise;
    std::shared_future<void> start = start_promise.get_future().share();
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t index = 0; index < thread_count; ++index) {
        threads.emplace_back([log = subject.log, start] {
            start.wait();
            const std::array<tpmkit::log_field, 1> fields{{{"thread", "worker"}}};
            log->log(tpmkit::log_level::info, "concurrent record",
                     gsl::span<const tpmkit::log_field>{fields});
        });
    }

    start_promise.set_value();
    for (std::thread& thread : threads) {
        thread.join();
    }

    const std::vector<std::string> records = subject.records();
    if (!subject.emits_records) {
        EXPECT_TRUE(records.empty());
        return;
    }

    ASSERT_EQ(records.size(), thread_count);
    for (const std::string& record : records) {
        EXPECT_NE(record.find("concurrent record"), std::string::npos);
    }
}

std::string logger_case_name(const testing::TestParamInfo<logger_adapter_case>& info)
{
    return info.param.name;
}

std::vector<logger_adapter_case> logger_cases()
{
    std::vector<logger_adapter_case> cases{
        logger_adapter_case{
            "noop_logger",
            make_noop_case,
        },
        logger_adapter_case{
            "recording_logger",
            make_recording_case,
        },
    };
#ifdef TPMKIT_HAS_SPDLOG_ADAPTER
    cases.push_back(logger_adapter_case{
        "spdlog_logger",
        make_spdlog_case,
    });
#endif
#ifdef TPMKIT_HAS_STDIO_ADAPTER
    cases.push_back(logger_adapter_case{
        "stdio_logger",
        make_stdio_case,
    });
#endif
    return cases;
}

INSTANTIATE_TEST_SUITE_P(logger_adapters, logger_port, testing::ValuesIn(logger_cases()),
                         logger_case_name);

} // namespace
