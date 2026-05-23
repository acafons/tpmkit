#include <tpmkit/exception.h>
#include <tpmkit/logging/logger.h>
#include <tpmkit/logging/noop_logger.h>
#include <tpmkit/result.h>

int tpmkit_foundational_headers_smoke()
{
    tpmkit::noop_logger noop;
    tpmkit::logger* const log = &noop;
    const tpmkit::outcome<int> result{1};
    const tpmkit::tpmkit_error error{"x"};

    log->log(tpmkit::log_level::info, error.what(), gsl::span<const tpmkit::log_field>());

    return result.value();
}
