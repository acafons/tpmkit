#include <tpmkit/logger.h>

namespace {

class smoke_logger final : public tpmkit::logger {
public:
    void log(
        tpmkit::log_level,
        std::string_view,
        gsl::span<const tpmkit::log_field>) noexcept final
    {
    }
};

} // namespace

int tpmkit_logger_header_smoke()
{
    smoke_logger log;
    const tpmkit::log_field field{"key", "value"};

    log.log(
        tpmkit::log_level::debug,
        "message",
        gsl::span<const tpmkit::log_field>(&field, 1));

    return 0;
}
