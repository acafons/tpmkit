#pragma once

/**
 * @file tpmkit/pcr_result_types.h
 * @brief Result data models returned by PCR provider operations.
 */

#include <tpmkit/api.h>
#include <tpmkit/pcr_digest_value.h>
#include <tpmkit/pcr_selection.h>

#include <cstdint>
#include <vector>

namespace tpmkit {

/**
 * @brief Result returned by a PCR read operation.
 *
 * @thread_safety Thread-compatible. Independent values may be used concurrently.
 * @exception_safety Follows member type guarantees.
 * @since v0.1
 */
struct pcr_read_result {
    /** @brief Selection the TPM actually read, which may differ from the request. */
    pcr_selection actual_selection;
    /** @brief TPM PCR generation counter reported with the read. */
    std::uint32_t update_counter;
    /** @brief Digest values returned for the selected PCRs. */
    std::vector<pcr_digest_value> values;
};

/**
 * @brief Compare PCR read results by all fields.
 */
[[nodiscard]] TPMKIT_API bool operator==(const pcr_read_result& lhs, const pcr_read_result& rhs);

/**
 * @brief Compare PCR read results by all fields.
 */
[[nodiscard]] TPMKIT_API bool operator!=(const pcr_read_result& lhs, const pcr_read_result& rhs);

/**
 * @brief Result returned by a PCR event operation.
 *
 * @thread_safety Thread-compatible. Independent values may be used concurrently.
 * @exception_safety Follows member type guarantees.
 * @since v0.1
 */
struct pcr_event_result {
    /** @brief Resulting digest values across active PCR banks. */
    std::vector<pcr_digest_value> digests;
};

/**
 * @brief Compare PCR event results by digest values.
 */
[[nodiscard]] TPMKIT_API bool operator==(const pcr_event_result& lhs, const pcr_event_result& rhs);

/**
 * @brief Compare PCR event results by digest values.
 */
[[nodiscard]] TPMKIT_API bool operator!=(const pcr_event_result& lhs, const pcr_event_result& rhs);

/**
 * @brief Result returned by a PCR allocation operation.
 *
 * @thread_safety Thread-compatible. Independent values may be used concurrently.
 * @exception_safety noexcept for comparisons; construction follows aggregate
 * initialization guarantees.
 * @since v0.1
 */
struct pcr_allocate_result {
    /** @brief Whether the TPM accepted the requested allocation. */
    bool allocation_success;
    /** @brief Maximum number of PCRs supported by the TPM. */
    std::uint32_t max_pcr;
    /** @brief NV space needed for the requested PCR allocation. */
    std::uint32_t size_needed;
    /** @brief NV space currently available for PCR allocation. */
    std::uint32_t size_available;
};

/**
 * @brief Compare PCR allocation results by all fields.
 */
[[nodiscard]] TPMKIT_API bool operator==(const pcr_allocate_result& lhs,
                                         const pcr_allocate_result& rhs) noexcept;

/**
 * @brief Compare PCR allocation results by all fields.
 */
[[nodiscard]] TPMKIT_API bool operator!=(const pcr_allocate_result& lhs,
                                         const pcr_allocate_result& rhs) noexcept;

} // namespace tpmkit
