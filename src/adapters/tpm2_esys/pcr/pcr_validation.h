#pragma once

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/bank.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/result.h>

#include <gsl/span>

#include <cstddef>
#include <cstdint>
#include <string_view>

typedef struct ESYS_CONTEXT ESYS_CONTEXT;

namespace tpmkit::detail::tpm2_esys {

[[nodiscard]] outcome<void> ensure_bank_count(gsl::span<const pcr::bank> banks);

[[nodiscard]] outcome<void> ensure_digest_count(gsl::span<const pcr::digest_value> digests);

[[nodiscard]] outcome<void> ensure_esys_available(ESYS_CONTEXT* esys);

[[nodiscard]] outcome<void> ensure_event_size(gsl::span<const std::uint8_t> event_data);

[[nodiscard]] outcome<void> ensure_policy_digest_size(hash_algorithm algorithm,
                                                      gsl::span<const std::uint8_t> digest);

[[nodiscard]] outcome<void> ensure_secure_auth_value_transport(gsl::span<const std::uint8_t> auth);

[[nodiscard]] outcome<void> ensure_sized_buffer_fits(std::size_t size, std::string_view message);

} // namespace tpmkit::detail::tpm2_esys
