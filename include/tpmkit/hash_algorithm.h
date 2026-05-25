#pragma once

/**
 * @file tpmkit/hash_algorithm.h
 * @brief Hash algorithm vocabulary shared by PCR domain types.
 */

#include <tpmkit/api.h>

#include <cstddef>

namespace tpmkit {

/**
 * @brief TPM-supported hash algorithms used for PCR banks and digest values.
 *
 * @thread_safety Enum values are immutable and thread-safe.
 * @exception_safety No operations throw.
 * @since v0.1
 */
enum class TPMKIT_API hash_algorithm {
    /** @brief SHA-1 digest algorithm. */
    sha1,
    /** @brief SHA-256 digest algorithm. */
    sha256,
    /** @brief SHA-384 digest algorithm. */
    sha384,
    /** @brief SHA-512 digest algorithm. */
    sha512,
};

/**
 * @brief Return the digest size in bytes for a hash algorithm.
 *
 * @param[in] algorithm Hash algorithm to inspect.
 * @return Digest size in bytes.
 * @throws tpmkit_error if `algorithm` is not one of the supported enumerators.
 * @thread_safety Thread-safe.
 * @exception_safety Strong; no state is modified.
 */
[[nodiscard]] TPMKIT_API std::size_t digest_size(hash_algorithm algorithm);

} // namespace tpmkit
