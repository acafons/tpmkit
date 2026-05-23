#include <tpmkit/logging/noop_logger.h>

int tpmkit_noop_logger_header_smoke()
{
    tpmkit::noop_logger noop;
    tpmkit::logger* const log = &noop;

    log->log(tpmkit::log_level::trace, "message", gsl::span<const tpmkit::log_field>());

    return 0;
}
