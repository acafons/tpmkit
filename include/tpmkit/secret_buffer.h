#pragma once

/**
 * @file tpmkit/secret_buffer.h
 * @brief Move-only owner for sensitive byte material.
 */

#include <tpmkit/api.h>

#include <gsl/span>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tpmkit {

/**
 * @brief Move-only buffer for secret byte material.
 *
 * The buffer attempts to lock its backing storage in RAM, clears its contents
 * with an anti-elision cleanse routine before release, and intentionally
 * exposes no string conversion or streaming surface.
 *
 * @thread_safety Thread-compatible. Independent instances may be used
 * concurrently; a shared instance requires external synchronization.
 * @exception_safety Destruction and moves are noexcept. Construction follows
 * the guarantees of `std::vector` move and implementation allocation.
 * @since v0.1
 */
class TPMKIT_API secret_buffer final {
public:
    /**
     * @brief Construct an empty secret buffer.
     *
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    secret_buffer() noexcept;

    /**
     * @brief Construct a secret buffer by taking ownership of byte material.
     *
     * @param[in] data Secret bytes to own. The source vector must be moved from.
     * @throws std::bad_alloc if internal allocation fails.
     * @thread_safety Thread-compatible.
     * @exception_safety Strong; failure does not publish a partially
     * initialized instance.
     */
    explicit secret_buffer(std::vector<std::uint8_t>&& data);

    /**
     * @brief Clear and release the secret buffer.
     *
     * @thread_safety Single-threaded for the destroyed instance.
     * @exception_safety noexcept.
     */
    ~secret_buffer() noexcept;

    /**
     * @brief Copy construction is disabled to avoid duplicating secrets.
     */
    secret_buffer(const secret_buffer& other) = delete;

    /**
     * @brief Copy assignment is disabled to avoid duplicating secrets.
     *
     * @return Reference to this buffer if the operation existed.
     */
    secret_buffer& operator=(const secret_buffer& other) = delete;

    /**
     * @brief Move-construct a secret buffer, transferring ownership.
     *
     * @param[in] other Source buffer. Empty after the move.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    secret_buffer(secret_buffer&& other) noexcept;

    /**
     * @brief Move-assign a secret buffer, clearing existing contents first.
     *
     * @param[in] other Source buffer. Empty after the move.
     * @return Reference to this buffer.
     * @thread_safety Single-threaded for the moved instances.
     * @exception_safety noexcept.
     */
    secret_buffer& operator=(secret_buffer&& other) noexcept;

    /**
     * @brief Return whether the buffer currently contains no bytes.
     *
     * @return True when empty.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief Return the number of secret bytes currently owned.
     *
     * @return Buffer size in bytes.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return a non-owning read-only view over the secret bytes.
     *
     * @return Span valid until the buffer is moved from or destroyed.
     * @thread_safety Thread-compatible.
     * @exception_safety noexcept.
     */
    [[nodiscard]] gsl::span<const std::uint8_t> view() const noexcept;

private:
    class impl;

    std::unique_ptr<impl> impl_;
};

} // namespace tpmkit
