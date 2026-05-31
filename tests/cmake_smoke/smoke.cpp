#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

int main()
{
    const tpmkit::tpm_context_config config;

    return config.tcti.config.empty() ? 0 : 1;
}
