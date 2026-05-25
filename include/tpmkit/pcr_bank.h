#pragma once

/**
 * @file tpmkit/pcr_bank.h
 * @brief PCR bank value object pairing a hash algorithm with digest length.
 */

#include <tpmkit/api.h>
#include <tpmkit/hash_algorithm.h>

#include <cstddef>

namespace tpmkit {

/**
 * @brief Immutable PCR bank descriptor.
 *
 * A PCR bank is identified by a hash algorithm and carries the digest size that
 * values in that bank must use.
 *
 * @thread_safety Thread-compatible. Instances are immutable and independent
 * instances may be used concurrently.
 * @exception_safety Construction provides the strong guarantee. Accessors and
 * comparisons are noexcept.
 * @since v0.1
 */
class TPMKIT_API pcr_bank final {
public:
    /**
     * @brief Construct a bank descriptor for a supported hash algorithm.
     *
     * @param[in] algorithm Hash algorithm that identifies the PCR bank.
     * @throws input_validation_error if `algorithm` is not supported.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    explicit pcr_bank(hash_algorithm algorithm);

    /**
     * @brief Return the bank hash algorithm.
     *
     * @return Algorithm associated with this bank.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] hash_algorithm algorithm() const noexcept;

    /**
     * @brief Return the required digest size for this bank.
     *
     * @return Digest size in bytes.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t digest_size() const noexcept;

    /** @brief Compare banks by algorithm and digest size. */
    [[nodiscard]] bool operator!=(const pcr_bank& other) const noexcept;

    /** @brief Compare banks by algorithm and digest size. */
    [[nodiscard]] bool operator==(const pcr_bank& other) const noexcept;

private:
    hash_algorithm algorithm_;
    std::size_t digest_size_;
};

} // namespace tpmkit
