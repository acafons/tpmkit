#include <tpmkit/pcr/result_types.h>

#include <cstdint>
#include <vector>

int tpmkit_pcr_result_types_header_self_contained()
{
    const tpmkit::pcr::read_result result{
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index::debug}},
        7U,
        std::vector<tpmkit::pcr::value>{
            tpmkit::pcr::value{
                tpmkit::pcr::index::debug,
                tpmkit::pcr::digest_value{tpmkit::hash_algorithm::sha256,
                                          std::vector<std::uint8_t>(32U)},
            },
        },
    };

    return result.update_counter == 7U ? 0 : 1;
}
