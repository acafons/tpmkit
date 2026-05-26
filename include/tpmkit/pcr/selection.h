#pragma once

/**
 * @file tpmkit/pcr/selection.h
 * @brief PCR selection value object for selecting indices within one bank.
 */

#include <tpmkit/api.h>
#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/index.h>

#include <initializer_list>
#include <set>

namespace tpmkit {

/**
 * @brief Immutable PCR selection for one hash algorithm bank.
 *
 * Holds a supported hash algorithm and a sorted unique set of PCR indices to
 * operate on within that bank. An empty selection is valid.
 *
 * @thread_safety Thread-compatible. Instances are immutable and independent
 * instances may be used concurrently.
 * @exception_safety Construction provides the strong guarantee. Accessors are
 * noexcept. Comparisons follow `std::set` guarantees.
 * @since v0.1
 */
class TPMKIT_API pcr_selection final {
public:
    /**
     * @brief Construct an empty selection for one bank.
     *
     * @param[in] algorithm Hash algorithm that identifies the PCR bank.
     * @throws input_validation_error if `algorithm` is not supported.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    explicit pcr_selection(hash_algorithm algorithm);

    /**
     * @brief Construct a selection from an initializer list of indices.
     *
     * @param[in] algorithm Hash algorithm that identifies the PCR bank.
     * @param[in] indices PCR indices to select. Duplicates are collapsed.
     * @throws input_validation_error if `algorithm` is not supported.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    pcr_selection(hash_algorithm algorithm, std::initializer_list<pcr_index> indices);

    /**
     * @brief Construct a selection from a set of indices.
     *
     * @param[in] algorithm Hash algorithm that identifies the PCR bank.
     * @param[in] indices PCR indices to select. Ownership is moved into the value object.
     * @throws input_validation_error if `algorithm` is not supported.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; invalid input does not create an instance.
     */
    pcr_selection(hash_algorithm algorithm, std::set<pcr_index> indices);

    /**
     * @brief Return the selection hash algorithm.
     *
     * @return Algorithm associated with this selection.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] hash_algorithm algorithm() const noexcept;

    /**
     * @brief Return selected PCR indices.
     *
     * @return Sorted unique set valid for the lifetime of this object.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] const std::set<pcr_index>& indices() const noexcept;

    /** @brief Compare selections by algorithm and index set. */
    [[nodiscard]] bool operator!=(const pcr_selection& other) const;

    /** @brief Compare selections by algorithm and index set. */
    [[nodiscard]] bool operator==(const pcr_selection& other) const;

private:
    hash_algorithm algorithm_;
    std::set<pcr_index> indices_;
};

} // namespace tpmkit
