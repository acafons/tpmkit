#include <tpmkit/logging/logger.h>
#ifdef TPMKIT_EXAMPLE_HAS_STDIO
#include <tpmkit/logging/stdio_logger.h>
#endif
#include <tpmkit/tpm_context.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view default_tcti_config = "tabrmd:bus_type=system";

std::string_view category_name(const tpmkit::error_category category) noexcept
{
    switch (category) {
    case tpmkit::error_category::input_error:
        return "input_error";
    case tpmkit::error_category::security_failure:
        return "security_failure";
    case tpmkit::error_category::resource_error:
        return "resource_error";
    case tpmkit::error_category::backend_error:
        return "backend_error";
    }

    return "unknown";
}

} // namespace

int main(const int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: " << argv[0] << " [tcti-config]\n";
        return EXIT_FAILURE;
    }

    std::string tcti_config{default_tcti_config};
    if (argc == 2) {
        tcti_config = argv[1];
    }

    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{std::move(tcti_config)};
    config.startup = tpmkit::tpm_context_config::startup_mode::clear;
#ifdef TPMKIT_EXAMPLE_HAS_STDIO
    config.log = std::make_shared<tpmkit::stdio_logger>();
#endif

    auto context = tpmkit::tpm_context::create(std::move(config));
    if (!context.has_value()) {
        std::cerr << category_name(context.error().category) << ": " << context.error().message
                  << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "tpm_context ready\n";
    return EXIT_SUCCESS;
}
