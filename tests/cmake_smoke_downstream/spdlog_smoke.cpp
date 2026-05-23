#include <tpmkit/logging/spdlog_logger.h>

#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

int main()
{
    auto sink = std::make_shared<::spdlog::sinks::null_sink_mt>();
    auto inner = std::make_shared<::spdlog::logger>("tpmkit", std::move(sink));
    tpmkit::spdlog_logger log{std::move(inner)};
    log.flush();

    return 0;
}
