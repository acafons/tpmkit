#include <tpmkit/pcr_index.h>

int tpmkit_pcr_index_header_self_contained()
{
    return tpmkit::pcr_index::debug.value() == 16U ? 0 : 1;
}
