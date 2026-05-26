#pragma once

/**
 * @file tpmkit/pcr/digest_value.h
 * @brief PCR digest value object with algorithm-aware size validation.
 */

#include <tpmkit/api.h>
#include <tpmkit/hash_algorithm.h>

#include <cstdint>
#include <vector>

namespace tpmkit::pcr {

/**
 * @brief Immutable digest value for a PCR bank.
 *
 * Pairs a hash algorithm with digest bytes and validates that the byte length
 * matches the algorithm's required digest size.
 *
 * @thread_safety Thread-compatible. Instances are immutable and independent
 * instances may be used concurrently.
 * @exception_safety Construction provides the strong guarantee. Accessors are
 * noexcept. Comparisons follow `std::vector` guarantees.
 * @since v0.1
 */
class TPMKIT_API digest_value final {
public:
    /**
     * @brief Construct a PCR digest value.
     *
     * @param[in] algorithm Hash algorithm that produced `digest`.
     * @param[in] digest Digest bytes. Size must match `algorithm`.
     * @throws input_validation_error if the algorithm is unsupported or the
     * digest size does not match the algorithm.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    digest_value(hash_algorithm algorithm, std::vector<std::uint8_t> digest);

    /**
     * @brief Return the digest hash algorithm.
     *
     * @return Algorithm associated with this digest.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] hash_algorithm algorithm() const noexcept;

    /**
     * @brief Return the digest bytes.
     *
     * @return Read-only reference valid for the lifetime of this object.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] const std::vector<std::uint8_t>& digest() const noexcept;

    /** @brief Compare digest values by algorithm and bytes. */
    [[nodiscard]] bool operator!=(const digest_value& other) const;

    /** @brief Compare digest values by algorithm and bytes. */
    [[nodiscard]] bool operator==(const digest_value& other) const;

private:
    hash_algorithm algorithm_;
    std::vector<std::uint8_t> digest_;
};

} // namespace tpmkit::pcr
