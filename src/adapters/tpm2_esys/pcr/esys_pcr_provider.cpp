#include "esys_pcr_provider.h"

#include "../support/error_translation.h"
#include "../support/log_events.h"
#include "pcr_logging.h"
#include "pcr_marshalling.h"
#include "pcr_validation.h"

#include <memory>

namespace tpmkit::detail::esys {
namespace {

using unique_digest_values = std::unique_ptr<TPML_DIGEST_VALUES, void (*)(void*) noexcept>;
using unique_pcr_selection = std::unique_ptr<TPML_PCR_SELECTION, void (*)(void*) noexcept>;
using unique_pcr_values = std::unique_ptr<TPML_DIGEST, void (*)(void*) noexcept>;

} // namespace

esys_pcr_provider::esys_pcr_provider(ESYS_CONTEXT* const esys, logger& log,
                                     pcr::observer* const observer) noexcept
    : esys_pcr_provider{esys, log, observer, default_esys_api()}
{}

esys_pcr_provider::esys_pcr_provider(ESYS_CONTEXT* const esys, logger& log,
                                     pcr::observer* const observer, const esys_api& api) noexcept
    : api_{api}, esys_{esys}, log_{log}, observer_{observer}
{}

outcome<pcr::allocate_result> esys_pcr_provider::allocate(const gsl::span<const pcr::bank> banks)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const auto valid_count = ensure_bank_count(banks);
    if (!valid_count.has_value()) {
        return tl::unexpected(valid_count.error());
    }

    const TPML_PCR_SELECTION allocation = to_tpm_allocation(banks);
    TPMI_YES_NO allocation_success = TPM2_NO;
    UINT32 max_pcr = 0U;
    UINT32 size_needed = 0U;
    UINT32 size_available = 0U;
    const TSS2_RC rc = api_.pcr_allocate(esys_, ESYS_TR_RH_PLATFORM, ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                         ESYS_TR_NONE, &allocation, &allocation_success, &max_pcr,
                                         &size_needed, &size_available);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated =
            translate_tss_rc(rc, "pcr_allocate", &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    pcr::allocate_result result{allocation_success != TPM2_NO, max_pcr, size_needed,
                                size_available};
    log_allocate_completed(log_, result.allocation_success, banks.size());
    return result;
}

outcome<pcr::event_result> esys_pcr_provider::event(const pcr::index index,
                                                    const gsl::span<const std::uint8_t> event_data)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const auto valid_size = ensure_event_size(event_data);
    if (!valid_size.has_value()) {
        return tl::unexpected(valid_size.error());
    }

    const TPM2B_EVENT tpm_event = to_tpm_event(event_data);
    TPML_DIGEST_VALUES* raw_digests = nullptr;
    const TSS2_RC rc = api_.pcr_event(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                      ESYS_TR_NONE, &tpm_event, &raw_digests);
    unique_digest_values digests{raw_digests, api_.free};
    if (rc != TSS2_RC_SUCCESS) {
        auto translated =
            translate_tss_rc(rc, "pcr_event", &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    if (digests == nullptr) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned no PCR event digests"});
    }

    auto domain_digests = to_domain_digests(*digests);
    if (!domain_digests.has_value()) {
        return tl::unexpected(domain_digests.error());
    }

    pcr::event_result result{*std::move(domain_digests)};
    log_event_completed(log_, index, event_data.size(), result.digests.size());
    if (observer_ != nullptr) {
        observer_->on_event(index, event_data, result);
    }

    return result;
}

outcome<void> esys_pcr_provider::extend(const pcr::index index,
                                        const gsl::span<const pcr::digest_value> digests)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const auto valid_count = ensure_digest_count(digests);
    if (!valid_count.has_value()) {
        return tl::unexpected(valid_count.error());
    }

    const TPML_DIGEST_VALUES tpm_digests = to_tpm_digest_values(digests);
    const TSS2_RC rc = api_.pcr_extend(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                       ESYS_TR_NONE, &tpm_digests);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated =
            translate_tss_rc(rc, "pcr_extend", &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    log_extend_completed(log_, index, digests.size());
    if (observer_ != nullptr) {
        observer_->on_extend(index, digests);
    }

    return {};
}

outcome<pcr::read_result> esys_pcr_provider::read(const pcr::selection& selection)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const TPML_PCR_SELECTION tpm_selection = to_tpm_selection(selection);
    UINT32 update_counter = 0U;
    TPML_PCR_SELECTION* raw_selection = nullptr;
    TPML_DIGEST* raw_values = nullptr;
    const TSS2_RC rc = api_.pcr_read(esys_, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                                     &tpm_selection, &update_counter, &raw_selection, &raw_values);
    unique_pcr_selection actual_selection{raw_selection, api_.free};
    unique_pcr_values values{raw_values, api_.free};
    if (rc != TSS2_RC_SUCCESS) {
        auto translated =
            translate_tss_rc(rc, "pcr_read", &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    if (actual_selection == nullptr || values == nullptr) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned incomplete PCR read response"});
    }

    auto domain_selection = to_domain_selection(*actual_selection, selection.algorithm());
    if (!domain_selection.has_value()) {
        return tl::unexpected(domain_selection.error());
    }

    auto domain_values = to_domain_read_values(*values, *domain_selection);
    if (!domain_values.has_value()) {
        return tl::unexpected(domain_values.error());
    }

    pcr::read_result result{*domain_selection, update_counter, *std::move(domain_values)};
    log_read_completed(log_, result.actual_selection, result.values.size());
    return result;
}

outcome<void> esys_pcr_provider::reset(const pcr::index index)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const TSS2_RC rc =
        api_.pcr_reset(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated =
            translate_tss_rc(rc, "pcr_reset", &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    log_reset_completed(log_, index);
    return {};
}

outcome<void> esys_pcr_provider::set_auth_policy(const pcr::index index,
                                                 const hash_algorithm policy_alg,
                                                 const gsl::span<const std::uint8_t> policy_digest)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const auto valid_digest = ensure_policy_digest_size(policy_alg, policy_digest);
    if (!valid_digest.has_value()) {
        return tl::unexpected(valid_digest.error());
    }

    const TPM2B_DIGEST auth_policy = to_tpm_sized_digest(policy_digest);
    const TSS2_RC rc = api_.pcr_set_auth_policy(
        esys_, ESYS_TR_RH_PLATFORM, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, &auth_policy,
        algorithm_to_tpm(policy_alg), static_cast<TPMI_DH_PCR>(TPM2_HR_PCR + index.value()));
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_set_auth_policy", &log_, events::pcr_tss_error,
                                           api_.decode_error);
        return tl::unexpected(translated.error());
    }

    log_auth_policy_set(log_, index, policy_alg);
    return {};
}

outcome<void> esys_pcr_provider::set_auth_value(const pcr::index index, secret_buffer auth)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const auto auth_view = auth.view();
    const auto valid_auth =
        ensure_sized_buffer_fits(auth_view.size(), "PCR auth value is too large");
    if (!valid_auth.has_value()) {
        return tl::unexpected(valid_auth.error());
    }

    const auto secure_transport = ensure_secure_auth_value_transport(auth_view);
    if (!secure_transport.has_value()) {
        return tl::unexpected(secure_transport.error());
    }

    const TPM2B_AUTH current_auth{};
    const TSS2_RC set_auth_rc = api_.tr_set_auth(esys_, pcr_handle(index), &current_auth);
    if (set_auth_rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(set_auth_rc, "pcr_set_auth_value_set_current_auth",
                                           &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    const TPM2B_DIGEST tpm_auth = to_tpm_sized_digest(auth_view);
    const TSS2_RC rc = api_.pcr_set_auth_value(esys_, pcr_handle(index), ESYS_TR_PASSWORD,
                                               ESYS_TR_NONE, ESYS_TR_NONE, &tpm_auth);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_set_auth_value", &log_, events::pcr_tss_error,
                                           api_.decode_error);
        return tl::unexpected(translated.error());
    }

    const TPM2B_AUTH new_auth{};
    const TSS2_RC update_auth_rc = api_.tr_set_auth(esys_, pcr_handle(index), &new_auth);
    if (update_auth_rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(update_auth_rc, "pcr_set_auth_value_update_current_auth",
                                           &log_, events::pcr_tss_error, api_.decode_error);
        return tl::unexpected(translated.error());
    }

    log_auth_value_set(log_, index);
    return {};
}

} // namespace tpmkit::detail::esys
