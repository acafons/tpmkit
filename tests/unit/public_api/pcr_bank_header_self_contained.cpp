#include <tpmkit/pcr_bank.h>

int tpmkit_pcr_bank_header_self_contained()
{
    const tpmkit::pcr_bank bank{tpmkit::hash_algorithm::sha256};

    return bank.digest_size() == 32U ? 0 : 1;
}
