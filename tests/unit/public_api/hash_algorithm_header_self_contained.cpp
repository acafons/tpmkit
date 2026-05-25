#include <tpmkit/hash_algorithm.h>

int tpmkit_hash_algorithm_header_self_contained()
{
    return tpmkit::digest_size(tpmkit::hash_algorithm::sha256) == 32U ? 0 : 1;
}
