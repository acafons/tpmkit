#include <tpmkit/logging/noop_logger.h>

int tpmkit_noop_logger_header_smoke()
{
    tpmkit::noop_logger noop;
    tpmkit::logger* const log = &noop;
    tpmkit::logger* const shared_log = &tpmkit::noop_logger::instance();

    log->log(tpmkit::log_level::trace, "message", gsl::span<const tpmkit::log_field>());
    shared_log->log(tpmkit::log_level::trace, "message",
                    gsl::span<const tpmkit::log_field>());

    return 0;
}
