#include <tpmkit/pcr_result_types.h>

#include <cstdint>
#include <vector>

int tpmkit_pcr_result_types_header_self_contained()
{
    const tpmkit::pcr_read_result result{
        tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr_index::debug}},
        7U,
        std::vector<tpmkit::pcr_value>{
            tpmkit::pcr_value{
                tpmkit::pcr_index::debug,
                tpmkit::pcr_digest_value{tpmkit::hash_algorithm::sha256,
                                         std::vector<std::uint8_t>(32U)},
            },
        },
    };

    return result.update_counter == 7U ? 0 : 1;
}
