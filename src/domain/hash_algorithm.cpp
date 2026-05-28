#include <tpmkit/hash_algorithm.h>

#include <tpmkit/exception.h>

namespace tpmkit {

std::string_view hash_algorithm_name(const hash_algorithm algorithm) noexcept
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        return "sha1";
    case hash_algorithm::sha256:
        return "sha256";
    case hash_algorithm::sha384:
        return "sha384";
    case hash_algorithm::sha512:
        return "sha512";
    }

    return "unknown";
}

std::size_t digest_size(const hash_algorithm algorithm)
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        return 20U;
    case hash_algorithm::sha256:
        return 32U;
    case hash_algorithm::sha384:
        return 48U;
    case hash_algorithm::sha512:
        return 64U;
    }

    throw tpmkit_error{"unsupported hash algorithm"};
}

} // namespace tpmkit
