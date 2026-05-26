#include <tpmkit/pcr/selection.h>

int tpmkit_pcr_selection_header_self_contained()
{
    const tpmkit::pcr::selection selection{tpmkit::hash_algorithm::sha256,
                                           {tpmkit::pcr::index::debug}};

    return selection.indices().size() == 1U ? 0 : 1;
}
