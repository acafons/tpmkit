#include <tpmkit/pcr/bank.h>

#include <tpmkit/exception.h>

namespace {

[[nodiscard]] std::size_t validate_digest_size(const tpmkit::hash_algorithm algorithm)
{
    try {
        return tpmkit::digest_size(algorithm);
    } catch (const tpmkit::tpmkit_error&) {
        throw tpmkit::input_validation_error{"unsupported PCR bank hash algorithm"};
    }
}

} // namespace

namespace tpmkit {

pcr_bank::pcr_bank(const hash_algorithm algorithm)
    : algorithm_{algorithm}, digest_size_{validate_digest_size(algorithm)}
{}

hash_algorithm pcr_bank::algorithm() const noexcept
{
    return algorithm_;
}

std::size_t pcr_bank::digest_size() const noexcept
{
    return digest_size_;
}

bool pcr_bank::operator!=(const pcr_bank& other) const noexcept
{
    return !(*this == other);
}

bool pcr_bank::operator==(const pcr_bank& other) const noexcept
{
    return algorithm_ == other.algorithm_ && digest_size_ == other.digest_size_;
}

} // namespace tpmkit
