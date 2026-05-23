#include <tpmkit/logging/logger.h>
#include <tpmkit/tpm_context.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace {

class stderr_logger final : public tpmkit::logger {
public:
    void log(const tpmkit::log_level level, const std::string_view message,
             const gsl::span<const tpmkit::log_field> fields) noexcept final
    {
        std::cerr << level_name(level) << " " << message;
        for (const auto& field : fields) {
            std::cerr << " " << field.key << "=" << field.value;
        }
        std::cerr << "\n";
    }

private:
    static std::string_view level_name(const tpmkit::log_level level) noexcept
    {
        switch (level) {
        case tpmkit::log_level::trace:
            return "trace";
        case tpmkit::log_level::debug:
            return "debug";
        case tpmkit::log_level::info:
            return "info";
        case tpmkit::log_level::warn:
            return "warn";
        case tpmkit::log_level::error:
            return "error";
        }

        return "unknown";
    }
};

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
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <tcti-config>\n";
        return EXIT_FAILURE;
    }

    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{argv[1]};
    config.startup = tpmkit::tpm_context_config::startup_mode::clear;
    config.log = std::make_shared<stderr_logger>();

    auto context = tpmkit::tpm_context::create(std::move(config));
    if (!context.has_value()) {
        std::cerr << category_name(context.error().category) << ": " << context.error().message
                  << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "tpm_context ready\n";
    return EXIT_SUCCESS;
}
