#include "pcr_validation.h"

#include "pcr_marshalling.h"

#include <tss2/tss2_tpm2_types.h>

#include <set>
#include <string>

namespace tpmkit::detail::esys {

outcome<void> ensure_bank_count(const gsl::span<const pcr::bank> banks)
{
    if (banks.empty()) {
        return tl::unexpected(
            error{error_category::input_error, "PCR allocate requires at least one bank"});
    }

    if (banks.size() > TPM2_NUM_PCR_BANKS) {
        return tl::unexpected(error{error_category::input_error, "too many PCR banks"});
    }

    std::set<hash_algorithm> algorithms;
    for (const auto& bank : banks) {
        if (!algorithms.insert(bank.algorithm()).second) {
            return tl::unexpected(error{error_category::input_error, "duplicate PCR bank"});
        }
    }

    return {};
}

outcome<void> ensure_digest_count(const gsl::span<const pcr::digest_value> digests)
{
    if (digests.empty()) {
        return tl::unexpected(
            error{error_category::input_error, "PCR extend requires at least one digest"});
    }

    if (digests.size() > TPM2_NUM_PCR_BANKS) {
        return tl::unexpected(error{error_category::input_error, "too many PCR digests"});
    }

    std::set<hash_algorithm> algorithms;
    for (const auto& digest : digests) {
        if (!algorithms.insert(digest.algorithm()).second) {
            return tl::unexpected(
                error{error_category::input_error, "duplicate PCR digest algorithm"});
        }
    }

    return {};
}

outcome<void> ensure_esys_available(ESYS_CONTEXT* const esys)
{
    if (esys == nullptr) {
        return tl::unexpected(
            error{error_category::resource_error, "ESYS PCR provider has no TPM context"});
    }

    return {};
}

outcome<void> ensure_event_size(const gsl::span<const std::uint8_t> event_data)
{
    if (event_data.size() > sizeof(TPM2B_EVENT::buffer)) {
        return tl::unexpected(error{error_category::input_error, "PCR event data is too large"});
    }

    return {};
}

outcome<void> ensure_policy_digest_size(const hash_algorithm algorithm,
                                        const gsl::span<const std::uint8_t> digest)
{
    if (!is_supported_algorithm(algorithm)) {
        return tl::unexpected(
            error{error_category::input_error, "unsupported PCR auth policy hash algorithm"});
    }

    if (digest.size() != digest_size(algorithm)) {
        return tl::unexpected(
            error{error_category::input_error, "PCR auth policy digest has wrong size"});
    }

    return ensure_sized_buffer_fits(digest.size(), "PCR auth policy digest is too large");
}

outcome<void> ensure_secure_auth_value_transport(const gsl::span<const std::uint8_t> auth)
{
    if (!auth.empty()) {
        return tl::unexpected(error{
            error_category::resource_error,
            "secure PCR auth value transport is unavailable for non-empty authorization values"});
    }

    return {};
}

outcome<void> ensure_sized_buffer_fits(const std::size_t size, const std::string_view message)
{
    if (size > sizeof(TPM2B_DIGEST::buffer)) {
        return tl::unexpected(error{error_category::input_error, std::string{message}});
    }

    return {};
}

} // namespace tpmkit::detail::esys
