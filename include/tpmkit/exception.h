#pragma once

/**
 * @file tpmkit/exception.h
 * @brief Exception base type for exceptional tpmkit failures.
 *
 * Expected runtime failures are reported with `outcome<T>`. Exceptions are
 * reserved for programmer errors, allocation failure, and contract violations.
 */

#include <tpmkit/api.h>

#include <stdexcept>
#include <string>

namespace tpmkit {

/**
 * @brief Base exception for tpmkit-specific exceptional failures.
 *
 * The MVP exposes only the base exception. More specific exception types may be
 * introduced when a public API has a distinct exceptional failure mode.
 *
 * @thread_safety Thread-compatible. Independent exception objects may be used
 * concurrently; a shared object follows `std::runtime_error` synchronization
 * requirements.
 * @exception_safety Construction follows `std::runtime_error`; destruction is
 * noexcept.
 * @since v0.1
 */
class TPMKIT_API tpmkit_error : public std::runtime_error {
public:
    /**
     * @brief Construct an exception with a string message.
     *
     * @param[in] message Human-readable diagnostic message. Must not contain
     *                    secret-derived data.
     * @throws std::bad_alloc if storing the message allocates and allocation
     *         fails.
     * @thread_safety Thread-safe for construction of independent objects.
     * @exception_safety Strong; failure to construct has no side effects.
     */
    explicit tpmkit_error(const std::string& message) : std::runtime_error(message) {}

    /**
     * @brief Construct an exception with a C string message.
     *
     * @param[in] message Null-terminated diagnostic message. Must not be null
     *                    and must not contain secret-derived data.
     * @throws std::bad_alloc if storing the message allocates and allocation
     *         fails.
     * @thread_safety Thread-safe for construction of independent objects.
     * @exception_safety Strong; failure to construct has no side effects.
     */
    explicit tpmkit_error(const char* message) : std::runtime_error(validate_message(message)) {}

private:
    static const char* validate_message(const char* message)
    {
        if (message == nullptr) {
            throw tpmkit_error{std::string{"tpmkit_error message must not be null"}};
        }

        return message;
    }
};

} // namespace tpmkit
