#pragma once

/**
 * @file tpmkit/encoding/hex.h
 * @brief Hexadecimal encoding helpers for non-secret byte buffers.
 */

#include <tpmkit/api.h>
#include <tpmkit/result.h>

#include <gsl/span>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tpmkit::encoding {

/**
 * @brief Encode bytes as lowercase hexadecimal text.
 *
 * Produces two lowercase hexadecimal digits for each input byte, with no
 * prefix, separators, or whitespace.
 *
 * @param[in] bytes Non-owning view of non-secret bytes to encode. The view may
 * be empty and is only read during the call.
 * @return Hexadecimal text owned by the caller.
 * @throws std::length_error if the encoded output size cannot be represented.
 * @throws std::bad_alloc if allocating the output string fails.
 * @warning Hex output is not redacted or sanitized. Do not write encoded secret
 * material to logs, exceptions, standard streams, or persistent diagnostics.
 * @thread_safety Thread-safe.
 * @exception_safety Strong; failures do not modify caller-owned input.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API std::string encode_hex(gsl::span<const std::uint8_t> bytes);

/**
 * @brief Decode hexadecimal text into bytes.
 *
 * Accepts uppercase and lowercase hexadecimal digits. The input must contain
 * exactly two hexadecimal digits per output byte.
 *
 * @param[in] text Hexadecimal text to decode. The view may be empty and is only
 * read during the call.
 * @return Decoded bytes, or `input_error` when `text` has odd length, contains
 * a non-hexadecimal character, or is too large to represent.
 * @throws std::bad_alloc if allocating the output vector fails.
 * @thread_safety Thread-safe.
 * @exception_safety Strong; failures do not modify caller-owned input.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API outcome<std::vector<std::uint8_t>> decode_hex(std::string_view text);

} // namespace tpmkit::encoding
