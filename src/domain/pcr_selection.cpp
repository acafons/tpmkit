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

namespace tpmkit {

pcr_selection::pcr_selection(const hash_algorithm algorithm)
    : pcr_selection{algorithm, std::set<pcr_index>{}}
{}

pcr_selection::pcr_selection(const hash_algorithm algorithm,
                             std::initializer_list<pcr_index> indices)
    : pcr_selection{algorithm, std::set<pcr_index>{indices}}
{}

pcr_selection::pcr_selection(const hash_algorithm algorithm, std::set<pcr_index> indices)
    : algorithm_{algorithm}, indices_{std::move(indices)}
{
    validate_algorithm(algorithm_);
}

hash_algorithm pcr_selection::algorithm() const noexcept
{
    return algorithm_;
}

const std::set<pcr_index>& pcr_selection::indices() const noexcept
{
    return indices_;
}

bool pcr_selection::operator!=(const pcr_selection& other) const
{
    return !(*this == other);
}

bool pcr_selection::operator==(const pcr_selection& other) const
{
    return algorithm_ == other.algorithm_ && indices_ == other.indices_;
}

} // namespace tpmkit
