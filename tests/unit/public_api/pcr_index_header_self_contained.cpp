#include <tpmkit/pcr/index.h>

int tpmkit_pcr_index_header_self_contained()
{
    const auto indices = tpmkit::pcr::make_index_range(16U, 2U);

    return tpmkit::pcr::index::debug.value() == 16U && indices.size() == 2U ? 0 : 1;
}
