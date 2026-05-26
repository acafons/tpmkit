#pragma once

/**
 * @file tpmkit/pcr/index.h
 * @brief PCR register index value object and well-known PCR constants.
 */

#include <tpmkit/api.h>

#include <cstdint>
#include <functional>

namespace tpmkit {

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
class TPMKIT_API pcr_index final {
public:
    /** @brief Maximum TPM PCR index accepted by this value object. */
    static constexpr std::uint8_t max_value = 31U;

    /** @brief PCR 23, application-defined measurement register. */
    static const pcr_index application;
    /** @brief PCR 8, bootloader measurement register. */
    static const pcr_index bootloader_8;
    /** @brief PCR 9, bootloader measurement register. */
    static const pcr_index bootloader_9;
    /** @brief PCR 16, debug and test-use resettable register. */
    static const pcr_index debug;
    /** @brief PCR 17, DRTM measurement register. */
    static const pcr_index drtm_17;
    /** @brief PCR 18, DRTM measurement register. */
    static const pcr_index drtm_18;
    /** @brief PCR 19, DRTM measurement register. */
    static const pcr_index drtm_19;
    /** @brief PCR 20, DRTM measurement register. */
    static const pcr_index drtm_20;
    /** @brief PCR 21, DRTM measurement register. */
    static const pcr_index drtm_21;
    /** @brief PCR 22, DRTM measurement register. */
    static const pcr_index drtm_22;
    /** @brief PCR 0, firmware measurement register. */
    static const pcr_index firmware_0;
    /** @brief PCR 1, firmware measurement register. */
    static const pcr_index firmware_1;
    /** @brief PCR 2, firmware measurement register. */
    static const pcr_index firmware_2;
    /** @brief PCR 3, firmware measurement register. */
    static const pcr_index firmware_3;
    /** @brief PCR 4, firmware measurement register. */
    static const pcr_index firmware_4;
    /** @brief PCR 5, firmware measurement register. */
    static const pcr_index firmware_5;
    /** @brief PCR 6, firmware measurement register. */
    static const pcr_index firmware_6;
    /** @brief PCR 7, firmware measurement register. */
    static const pcr_index firmware_7;
    /** @brief PCR 10, IMA and integrity measurement register. */
    static const pcr_index ima;
    /** @brief PCR 11, OS-specific measurement register. */
    static const pcr_index os_11;
    /** @brief PCR 12, OS-specific measurement register. */
    static const pcr_index os_12;
    /** @brief PCR 13, OS-specific measurement register. */
    static const pcr_index os_13;
    /** @brief PCR 14, OS-specific measurement register. */
    static const pcr_index os_14;
    /** @brief PCR 15, OS-specific measurement register. */
    static const pcr_index os_15;

    /**
     * @brief Construct a PCR index from a numeric value.
     *
     * @param[in] value PCR index. Valid range is [0, 31].
     * @throws input_validation_error if `value` is greater than 31.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    explicit pcr_index(std::uint32_t value);

    /**
     * @brief Return the numeric PCR index.
     *
     * @return PCR index in the range [0, 31].
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

    /** @brief Compare PCR indices by numeric value. */
    [[nodiscard]] bool operator!=(const pcr_index& other) const noexcept;

    /** @brief Order PCR indices by numeric value for sorted containers. */
    [[nodiscard]] bool operator<(const pcr_index& other) const noexcept;

    /** @brief Compare PCR indices by numeric value. */
    [[nodiscard]] bool operator==(const pcr_index& other) const noexcept;

private:
    struct unchecked_tag final {};

    constexpr pcr_index(std::uint8_t value, unchecked_tag) noexcept : value_{value} {}

    std::uint8_t value_;

    friend struct std::hash<pcr_index>;
};

} // namespace tpmkit

namespace std {

/**
 * @brief Hash support for `tpmkit::pcr_index`.
 */
template <> struct hash<tpmkit::pcr_index> {
    [[nodiscard]] std::size_t operator()(const tpmkit::pcr_index& index) const noexcept
    {
        return std::hash<std::uint8_t>{}(index.value_);
    }
};

} // namespace std
