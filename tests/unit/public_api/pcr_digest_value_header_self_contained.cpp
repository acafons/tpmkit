#include <tpmkit/pcr/digest_value.h>

#include <cstdint>
#include <vector>

int tpmkit_pcr_digest_value_header_self_contained()
{
    const tpmkit::pcr::digest_value value{tpmkit::hash_algorithm::sha256,
                                          std::vector<std::uint8_t>(32U)};

    return value.digest().size() == 32U ? 0 : 1;
}
