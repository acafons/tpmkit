#include <tpmkit/logging/stdio_logger.h>

#include <memory>
#include <sstream>
#include <string>

int main()
{
    auto out = std::make_shared<std::ostringstream>();
    auto err = std::make_shared<std::ostringstream>();
    tpmkit::stdio_logger_options options;
    options.color = tpmkit::color_mode::never;
    options.out = out.get();
    options.err = err.get();

    tpmkit::stdio_logger log{options};
    log.log(tpmkit::log_level::info, "downstream stdio smoke",
            gsl::span<const tpmkit::log_field>{});

    return out->str().find("downstream stdio smoke") == std::string::npos ? 1 : 0;
}
