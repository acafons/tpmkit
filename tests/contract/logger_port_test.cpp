#include <tpmkit/noop_logger.h>
#include <tpmkit/testing/recording_logger.h>

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace {

struct logger_adapter_case {
    const char* name;
    std::function<std::shared_ptr<tpmkit::logger>()> create;
};

void PrintTo(const logger_adapter_case& test_case, std::ostream* const output)
{
    *output << test_case.name;
}

void log_through_port(tpmkit::logger& log)
{
    log.log(tpmkit::log_level::info, "through port", gsl::span<const tpmkit::log_field>{});
}

class logger_port : public testing::TestWithParam<logger_adapter_case> {};

TEST_P(logger_port, accepts_log_calls_through_reference_shape)
{
    // Verifies each logger adapter accepts calls through the logger port.

    const logger_adapter_case& test_case = GetParam();
    std::shared_ptr<tpmkit::logger> log = test_case.create();
    const std::array<tpmkit::log_field, 1> fields{{{"key", "value"}}};

    EXPECT_NO_THROW(
        log->log(tpmkit::log_level::trace, "ignored", gsl::span<const tpmkit::log_field>{fields}));
    EXPECT_NO_THROW(log_through_port(*log));
}

TEST_P(logger_port, is_assignable_to_shared_ptr_port_shape)
{
    // Verifies each logger adapter can be owned behind the logger port.

    const logger_adapter_case& test_case = GetParam();

    const std::shared_ptr<tpmkit::logger> log = test_case.create();

    ASSERT_NE(log, nullptr);
    EXPECT_NO_THROW(log_through_port(*log));
}

std::string logger_case_name(const testing::TestParamInfo<logger_adapter_case>& info)
{
    return info.param.name;
}

INSTANTIATE_TEST_SUITE_P(logger_adapters, logger_port,
                         testing::Values(
                             logger_adapter_case{
                                 "noop_logger",
                                 [] { return std::make_shared<tpmkit::noop_logger>(); },
                             },
                             logger_adapter_case{
                                 "recording_logger",
                                 [] {
                                     return std::make_shared<tpmkit::testing::recording_logger>();
                                 },
                             }),
                         logger_case_name);

} // namespace
