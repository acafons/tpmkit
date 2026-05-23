#include <tpmkit/logging/spdlog_logger.h>

int tpmkit_spdlog_logger_header_self_contained()
{
    // Verifies spdlog_logger.h compiles without pulling in spdlog headers.
    return 0;
}
