#include <tpmkit/pcr/index.h>

int tpmkit_pcr_index_header_self_contained()
{
    return tpmkit::pcr::index::debug.value() == 16U ? 0 : 1;
}
