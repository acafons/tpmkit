#include <tpmkit/pcr/provider.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

int main()
{
    const tpmkit::tpm_context_config config;
    const tpmkit::pcr_selection selection{tpmkit::hash_algorithm::sha256,
                                          {tpmkit::pcr_index::debug}};

    return std::holds_alternative<tpmkit::tcti_string_config>(config.tcti) &&
                   selection.algorithm() == tpmkit::hash_algorithm::sha256
               ? 0
               : 1;
}
