#include <tpmkit/pcr/digest_value.h>

#include <tpmkit/exception.h>

#include <utility>

namespace {

[[nodiscard]] std::size_t validate_digest_size(const tpmkit::hash_algorithm algorithm)
{
    try {
        return tpmkit::digest_size(algorithm);
    } catch (const tpmkit::tpmkit_error&) {
        throw tpmkit::input_validation_error{"unsupported PCR digest hash algorithm"};
    }
}

} // namespace

namespace tpmkit {

pcr_digest_value::pcr_digest_value(const hash_algorithm algorithm, std::vector<std::uint8_t> digest)
    : algorithm_{algorithm}, digest_{std::move(digest)}
{
    if (digest_.size() != validate_digest_size(algorithm_)) {
        throw input_validation_error{"PCR digest size does not match hash algorithm"};
    }
}

hash_algorithm pcr_digest_value::algorithm() const noexcept
{
    return algorithm_;
}

const std::vector<std::uint8_t>& pcr_digest_value::digest() const noexcept
{
    return digest_;
}

bool pcr_digest_value::operator!=(const pcr_digest_value& other) const
{
    return !(*this == other);
}

bool pcr_digest_value::operator==(const pcr_digest_value& other) const
{
    return algorithm_ == other.algorithm_ && digest_ == other.digest_;
}

} // namespace tpmkit
