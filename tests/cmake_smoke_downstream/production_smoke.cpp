#include <tpmkit/pcr/provider.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

int main()
{
    const tpmkit::tpm_context_config config;
    const tpmkit::pcr::selection selection{tpmkit::hash_algorithm::sha256,
                                           {tpmkit::pcr::index::debug}};

    return config.tcti.config.empty() && selection.algorithm() == tpmkit::hash_algorithm::sha256
               ? 0
               : 1;
}
