#include <tpmkit/tpm_context.h>

int tpmkit_tpm_context_header_self_contained()
{
    tpmkit::tpm_context_config config;

    return config.startup == tpmkit::tpm_context_config::startup_mode::clear ? 0 : 1;
}
