#include <tpmkit/testing/recording_logger.h>

int tpmkit_recording_logger_header_self_contained()
{
    tpmkit::testing::recording_logger log;
    return log.snapshot().empty() ? 0 : 1;
}
