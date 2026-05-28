#pragma once

/**
 * @file tpmkit/pcr/index.h
 * @brief PCR register index value object, range helper, and well-known PCR constants.
 */

#include <tpmkit/api.h>

#include <cstdint>
#include <functional>
#include <set>

namespace tpmkit::pcr {

/**
 * @brief Immutable TPM PCR register index.
 *
 * Wraps a PCR index in the TPM 2.0 valid range [0, 31]. Named constants cover
 * common platform roles and avoid repeated validation for those well-known
 * values.
 *
 * @thread_safety Thread-compatible. Instances are immutable and independent
 * instances may be used concurrently.
 * @exception_safety Construction provides the strong guarantee. Accessors and
 * comparisons are noexcept.
 * @since v0.1
 */
class TPMKIT_API index final {
public:
    /** @brief Maximum TPM PCR index accepted by this value object. */
    static constexpr std::uint8_t max_value = 31U;

    /** @brief PCR 23, application-defined measurement register. */
    static const index application;
    /** @brief PCR 8, bootloader measurement register. */
    static const index bootloader_8;
    /** @brief PCR 9, bootloader measurement register. */
    static const index bootloader_9;
    /** @brief PCR 16, debug and test-use resettable register. */
    static const index debug;
    /** @brief PCR 17, DRTM measurement register. */
    static const index drtm_17;
    /** @brief PCR 18, DRTM measurement register. */
    static const index drtm_18;
    /** @brief PCR 19, DRTM measurement register. */
    static const index drtm_19;
    /** @brief PCR 20, DRTM measurement register. */
    static const index drtm_20;
    /** @brief PCR 21, DRTM measurement register. */
    static const index drtm_21;
    /** @brief PCR 22, DRTM measurement register. */
    static const index drtm_22;
    /** @brief PCR 0, firmware measurement register. */
    static const index firmware_0;
    /** @brief PCR 1, firmware measurement register. */
    static const index firmware_1;
    /** @brief PCR 2, firmware measurement register. */
    static const index firmware_2;
    /** @brief PCR 3, firmware measurement register. */
    static const index firmware_3;
    /** @brief PCR 4, firmware measurement register. */
    static const index firmware_4;
    /** @brief PCR 5, firmware measurement register. */
    static const index firmware_5;
    /** @brief PCR 6, firmware measurement register. */
    static const index firmware_6;
    /** @brief PCR 7, firmware measurement register. */
    static const index firmware_7;
    /** @brief PCR 10, IMA and integrity measurement register. */
    static const index ima;
    /** @brief PCR 11, OS-specific measurement register. */
    static const index os_11;
    /** @brief PCR 12, OS-specific measurement register. */
    static const index os_12;
    /** @brief PCR 13, OS-specific measurement register. */
    static const index os_13;
    /** @brief PCR 14, OS-specific measurement register. */
    static const index os_14;
    /** @brief PCR 15, OS-specific measurement register. */
    static const index os_15;

    /**
     * @brief Construct a PCR index from a numeric value.
     *
     * @param[in] value PCR index. Valid range is [0, 31].
     * @throws input_validation_error if `value` is greater than 31.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    explicit index(std::uint32_t value);

    /**
     * @brief Return the numeric PCR index.
     *
     * @return PCR index in the range [0, 31].
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

    /** @brief Compare PCR indices by numeric value. */
    [[nodiscard]] bool operator!=(const index& other) const noexcept;

    /** @brief Order PCR indices by numeric value for sorted containers. */
    [[nodiscard]] bool operator<(const index& other) const noexcept;

    /** @brief Compare PCR indices by numeric value. */
    [[nodiscard]] bool operator==(const index& other) const noexcept;

private:
    struct unchecked_tag final {};

    constexpr index(std::uint8_t value, unchecked_tag) noexcept : value_{value} {}

    std::uint8_t value_;

    friend struct std::hash<index>;
};

/**
 * @brief Build a sorted set of contiguous PCR indices.
 *
 * Creates the half-open range `[first, first + count)`. Passing `count == 0`
 * returns an empty set and does not validate `first`.
 *
 * @param[in] first First PCR index in the range. When `count` is nonzero, it
 * must be in the valid PCR index range [0, 31].
 * @param[in] count Number of PCR indices to include. The resulting range must
 * not exceed `index::max_value`.
 * @return Sorted unique set of PCR indices in ascending order.
 * @throws input_validation_error if the non-empty range exceeds the supported
 * PCR index range.
 * @thread_safety Thread-safe.
 * @exception_safety Strong; invalid input does not modify caller state.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API std::set<index> make_index_range(std::uint32_t first, std::uint32_t count);

} // namespace tpmkit::pcr

namespace std {

/**
 * @brief Hash support for `tpmkit::pcr::index`.
 */
template <> struct hash<tpmkit::pcr::index> {
    [[nodiscard]] std::size_t operator()(const tpmkit::pcr::index& index) const noexcept
    {
        return std::hash<std::uint8_t>{}(index.value_);
    }
};

} // namespace std
