#include <tpmkit/hash_algorithm.h>

int tpmkit_hash_algorithm_header_self_contained()
{
    const std::string_view name = tpmkit::hash_algorithm_name(tpmkit::hash_algorithm::sha256);

    return tpmkit::digest_size(tpmkit::hash_algorithm::sha256) == 32U &&
                   name == std::string_view{"sha256"}
               ? 0
               : 1;
}
