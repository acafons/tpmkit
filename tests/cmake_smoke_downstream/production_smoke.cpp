#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

int main()
{
    const tpmkit::tpm_context_config config;

    return std::holds_alternative<tpmkit::tcti_string_config>(config.tcti) ? 0 : 1;
}
