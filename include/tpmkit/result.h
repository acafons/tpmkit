#pragma once

/**
 * @file tpmkit/result.h
 * @brief Result and error vocabulary shared by public fallible APIs.
 *
 * Declares the closed error category set, the value returned on failure, and
 * the `outcome<T>` alias used by APIs that report expected failures without
 * throwing exceptions.
 */

#include <tpmkit/api.h>

#include <tl/expected.hpp>

#include <string>
#include <string_view>

namespace tpmkit {

/**
 * @brief Stable categories used for programmatic error handling.
 *
 * Callers branch on these values rather than parsing error messages or native
 * backend status codes.
 *
 * @thread_safety Enum values are immutable and thread-safe.
 * @exception_safety No operations throw.
 * @since v0.1
 */
enum class error_category {
    /** @brief Caller supplied invalid arguments or malformed input. */
    input_error,
    /** @brief Security verification failed; caller-visible detail remains generic. */
    security_failure,
    /** @brief Required TPM, memory, file, or transport resource was unavailable. */
    resource_error,
    /** @brief Third-party backend returned an unexpected or opaque failure. */
    backend_error,
};

/**
 * @brief Return the stable lowercase name for an error category.
 *
 * @param[in] category Error category to name. Any value is accepted;
 * unsupported enum values return `"unknown"`.
 * @return Non-owning view of a static, null-terminated category name.
 * @thread_safety Thread-safe.
 * @exception_safety noexcept.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API std::string_view error_category_name(error_category category) noexcept;

/**
 * @brief Domain error returned by `outcome<T>` on expected failure.
 *
 * The category is the only programmatic discriminator. The message is
 * human-readable diagnostic text and must not be parsed by callers.
 *
 * @thread_safety Thread-compatible. Independent values may be used
 * concurrently; a shared value requires external synchronization.
 * @exception_safety Follows the guarantees of `std::string` member operations.
 * @since v0.1
 */
struct error {
    /** @brief Programmatic category for switch/if dispatch. */
    error_category category;
    /** @brief Human-readable diagnostic text, not a stable machine interface. */
    std::string message;
};

/**
 * @brief Expected-like result type used by fallible tpmkit APIs.
 *
 * `T` is the successful value type stored when the operation completes.
 * It contains `T` on success, or `error` on expected failure. Functions
 * returning this alias document their concrete returnable `error_category` set
 * at the declaration.
 * @thread_safety Matches `tl::expected<T, error>` for the chosen `T`.
 * @exception_safety Matches `tl::expected<T, error>` for the chosen `T`.
 * @since v0.1
 */
template <class T> using outcome = tl::expected<T, error>;

} // namespace tpmkit
