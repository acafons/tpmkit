#pragma once

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/bank.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/result_types.h>
#include <tpmkit/pcr/selection.h>
#include <tpmkit/result.h>

#include <gsl/span>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tpm2_types.h>

#include <cstdint>
#include <vector>

namespace tpmkit::detail::esys {

[[nodiscard]] outcome<hash_algorithm> algorithm_from_tpm(TPMI_ALG_HASH algorithm);

[[nodiscard]] TPMI_ALG_HASH algorithm_to_tpm(hash_algorithm algorithm) noexcept;

[[nodiscard]] bool is_supported_algorithm(hash_algorithm algorithm) noexcept;

[[nodiscard]] ESYS_TR pcr_handle(pcr::index index) noexcept;

[[nodiscard]] outcome<std::vector<pcr::digest_value>>
to_domain_digests(const TPML_DIGEST_VALUES& digests);

[[nodiscard]] outcome<std::vector<pcr::value>>
to_domain_read_values(const TPML_DIGEST& values, const pcr::selection& selection);

[[nodiscard]] outcome<pcr::selection> to_domain_selection(const TPML_PCR_SELECTION& selection,
                                                          hash_algorithm fallback_algorithm);

[[nodiscard]] TPML_PCR_SELECTION to_tpm_allocation(gsl::span<const pcr::bank> banks) noexcept;

[[nodiscard]] TPML_DIGEST_VALUES to_tpm_digest_values(gsl::span<const pcr::digest_value> digests);

[[nodiscard]] TPM2B_EVENT to_tpm_event(gsl::span<const std::uint8_t> event_data) noexcept;

[[nodiscard]] TPML_PCR_SELECTION to_tpm_selection(const pcr::selection& selection) noexcept;

[[nodiscard]] TPM2B_DIGEST to_tpm_sized_digest(gsl::span<const std::uint8_t> bytes) noexcept;

} // namespace tpmkit::detail::esys
