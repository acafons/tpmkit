#include <tpmkit/pcr/selection.h>

#include <tpmkit/exception.h>

#include <utility>

namespace {

void validate_algorithm(const tpmkit::hash_algorithm algorithm)
{
    try {
        static_cast<void>(tpmkit::digest_size(algorithm));
    } catch (const tpmkit::tpmkit_error&) {
        throw tpmkit::input_validation_error{"unsupported PCR selection hash algorithm"};
    }
}

} // namespace

namespace tpmkit::pcr {

selection::selection(const hash_algorithm algorithm)
    : selection{algorithm, std::set<index>{}}
{}

selection::selection(const hash_algorithm algorithm, std::initializer_list<index> indices)
    : selection{algorithm, std::set<index>{indices}}
{}

selection::selection(const hash_algorithm algorithm, std::set<index> indices)
    : algorithm_{algorithm}, indices_{std::move(indices)}
{
    validate_algorithm(algorithm_);
}

hash_algorithm selection::algorithm() const noexcept
{
    return algorithm_;
}

const std::set<index>& selection::indices() const noexcept
{
    return indices_;
}

bool selection::operator!=(const selection& other) const
{
    return !(*this == other);
}

bool selection::operator==(const selection& other) const
{
    return algorithm_ == other.algorithm_ && indices_ == other.indices_;
}

} // namespace tpmkit::pcr
