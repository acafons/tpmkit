#pragma once

/**
 * @file tpmkit/testing/fake_tcti.h
 * @brief Queue-programmed TSS2 TCTI substitute for tests.
 *
 * Declares a fake TCTI implementation used to exercise ESYS-facing code
 * without a live TPM endpoint.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Opaque TPM2 TSS TCTI context type returned by fake_tcti::handle().
 *
 * The concrete ABI layout is provided by TPM2 TSS headers in implementation
 * files; this public header only needs the opaque handle type.
 *
 * @thread_safety Follows the fake_tcti instance that owns the handle.
 * @exception_safety No operations are exposed by this forward declaration.
 */
typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;

/**
 * @namespace tpmkit::testing
 * @brief Public test substitutes and adapters for tpmkit consumers.
 *
 * @warning Experimental - API may change before 1.0.0.
 */
namespace tpmkit::testing {

/**
 * @brief Queue-programmed TSS2_TCTI v1 substitute for tests.
 *
 * fake_tcti exposes an opaque `TSS2_TCTI_CONTEXT*` handle backed by a FIFO of
 * scripted responses and failures. It is intended for tests that need to drive
 * real ESYS lifecycle code without swtpm or hardware.
 *
 * @warning Experimental - API may change before 1.0.0.
 * @thread_safety Thread-safe for queue programming and counters. The returned
 * TCTI handle follows the TSS contract and should be used by one ESYS context
 * at a time.
 * @exception_safety Destructor and move operations are noexcept; queue
 * programming provides the basic guarantee.
 * @since v0.1
 */
class fake_tcti final {
public:
    /**
     * @brief Construct an empty fake TCTI.
     *
     * @throws std::bad_alloc if allocating internal storage fails.
     * @thread_safety Thread-safe for construction of independent objects.
     * @exception_safety Strong; failure leaves no partially constructed object.
     */
    fake_tcti();

    /**
     * @brief Destroy the fake TCTI.
     *
     * @thread_safety Single-threaded for the destroyed instance.
     * @exception_safety noexcept.
     */
    ~fake_tcti();

    /**
     * @brief Copy construction is disabled because the fake owns a TCTI handle.
     *
     * @param[in] other Source fake; copying is intentionally unavailable.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    fake_tcti(const fake_tcti& other) = delete;

    /**
     * @brief Copy assignment is disabled because the fake owns a TCTI handle.
     *
     * @param[in] other Source fake; copying is intentionally unavailable.
     * @return Reference to this fake if the operation existed.
     * @thread_safety Not applicable.
     * @exception_safety Unavailable.
     */
    fake_tcti& operator=(const fake_tcti& other) = delete;

    /**
     * @brief Move-construct a fake TCTI.
     *
     * @param[in] other Source fake. Valid but unspecified after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    fake_tcti(fake_tcti&& other) noexcept;

    /**
     * @brief Move-assign a fake TCTI.
     *
     * @param[in] other Source fake. Valid but unspecified after the move.
     * @return Reference to this fake.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    fake_tcti& operator=(fake_tcti&& other) noexcept;

    /**
     * @brief Return the embedded TCTI context.
     *
     * @return Non-owning TCTI handle stable for this fake_tcti instance
     * lifetime.
     * @thread_safety Thread-compatible for the returned handle.
     * @exception_safety noexcept.
     */
    [[nodiscard]] TSS2_TCTI_CONTEXT* handle() noexcept;

    /**
     * @brief Return the number of queued receive outcomes not yet consumed.
     *
     * @return Count of pending scripted responses or failures.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t pending_responses() const noexcept;

    /**
     * @brief Queue one TSS return code for the next transmit/receive cycle.
     *
     * @param[in] tss_rc TSS2 return code surfaced by the next scripted failure.
     * @thread_safety Thread-safe.
     * @exception_safety Basic; allocation failure leaves the fake valid.
     */
    void push_failure(std::uint32_t tss_rc);

    /**
     * @brief Queue one response byte sequence for the next transmit/receive
     * cycle.
     *
     * @param[in] bytes Response bytes copied into the scripted FIFO.
     * @thread_safety Thread-safe.
     * @exception_safety Basic; allocation failure leaves the fake valid.
     */
    void push_response(std::vector<std::uint8_t> bytes);

    /**
     * @brief Return the number of transmit calls observed by the fake.
     *
     * @return Count of TCTI transmit callbacks observed since construction.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t transmits_observed() const noexcept;

    /**
     * @brief Return the number of finalize calls observed by the fake.
     *
     * @return Count of TCTI finalize callbacks observed since construction.
     * @thread_safety Thread-safe.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t finalizes_observed() const noexcept;

private:
    class impl;

    std::unique_ptr<impl> impl_;
};

} // namespace tpmkit::testing
