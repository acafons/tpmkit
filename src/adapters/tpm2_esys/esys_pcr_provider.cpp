#include "esys_pcr_provider.h"

#include "error_translation.h"
#include "log_events.h"

#include <tpmkit/logging/logger.h>

#include <tss2/tss2_esys.h>
#include <tss2/tss2_tpm2_types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tpmkit::detail::esys {
namespace {

using unique_digest_values = std::unique_ptr<TPML_DIGEST_VALUES, void (*)(void*)>;
using unique_pcr_selection = std::unique_ptr<TPML_PCR_SELECTION, void (*)(void*)>;
using unique_pcr_values = std::unique_ptr<TPML_DIGEST, void (*)(void*)>;

constexpr std::uint8_t pcr_select_min_size = 3U;

template <class Element, std::size_t Size>
[[nodiscard]] constexpr std::size_t array_size(const Element (&)[Size]) noexcept
{
    return Size;
}

[[nodiscard]] constexpr std::uint8_t selection_size_for_pcr(const std::uint8_t pcr) noexcept
{
    const auto required_size = static_cast<std::uint8_t>((pcr / 8U) + 1U);
    return std::max(pcr_select_min_size, required_size);
}

[[nodiscard]] std::string algorithm_name(const hash_algorithm algorithm)
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        return "sha1";
    case hash_algorithm::sha256:
        return "sha256";
    case hash_algorithm::sha384:
        return "sha384";
    case hash_algorithm::sha512:
        return "sha512";
    }

    return "unknown";
}

[[nodiscard]] TPMI_ALG_HASH algorithm_to_tpm(const hash_algorithm algorithm) noexcept
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        return TPM2_ALG_SHA1;
    case hash_algorithm::sha256:
        return TPM2_ALG_SHA256;
    case hash_algorithm::sha384:
        return TPM2_ALG_SHA384;
    case hash_algorithm::sha512:
        return TPM2_ALG_SHA512;
    }

    return TPM2_ALG_ERROR;
}

[[nodiscard]] bool is_supported_algorithm(const hash_algorithm algorithm) noexcept
{
    return algorithm_to_tpm(algorithm) != TPM2_ALG_ERROR;
}

[[nodiscard]] outcome<hash_algorithm> algorithm_from_tpm(const TPMI_ALG_HASH algorithm)
{
    switch (algorithm) {
    case TPM2_ALG_SHA1:
        return hash_algorithm::sha1;
    case TPM2_ALG_SHA256:
        return hash_algorithm::sha256;
    case TPM2_ALG_SHA384:
        return hash_algorithm::sha384;
    case TPM2_ALG_SHA512:
        return hash_algorithm::sha512;
    default:
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned unsupported PCR hash algorithm"});
    }
}

[[nodiscard]] outcome<void> ensure_esys_available(ESYS_CONTEXT* const esys)
{
    if (esys == nullptr) {
        return tl::unexpected(
            error{error_category::resource_error, "ESYS PCR provider has no TPM context"});
    }

    return {};
}

[[nodiscard]] outcome<void> ensure_event_size(const gsl::span<const std::uint8_t> event_data)
{
    if (event_data.size() > sizeof(TPM2B_EVENT::buffer)) {
        return tl::unexpected(error{error_category::input_error, "PCR event data is too large"});
    }

    return {};
}

[[nodiscard]] outcome<void> ensure_digest_count(const gsl::span<const pcr_digest_value> digests)
{
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

[[nodiscard]] outcome<void> ensure_bank_count(const gsl::span<const pcr_bank> banks)
{
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

[[nodiscard]] outcome<void> ensure_sized_buffer_fits(const std::size_t size,
                                                     const std::string_view message)
{
    if (size > sizeof(TPM2B_DIGEST::buffer)) {
        return tl::unexpected(error{error_category::input_error, std::string{message}});
    }

    return {};
}

[[nodiscard]] outcome<void>
ensure_secure_auth_value_transport(const gsl::span<const std::uint8_t> auth)
{
    if (!auth.empty()) {
        return tl::unexpected(error{
            error_category::resource_error,
            "secure PCR auth value transport is unavailable for non-empty authorization values"});
    }

    return {};
}

[[nodiscard]] outcome<void> ensure_policy_digest_size(const hash_algorithm algorithm,
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

[[nodiscard]] outcome<pcr_digest_value>
make_digest_value(const hash_algorithm algorithm, const BYTE* const data, const std::size_t size)
{
    if (data == nullptr && size > 0U) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned malformed PCR digest"});
    }

    if (size != digest_size(algorithm)) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned PCR digest with unexpected size"});
    }

    return pcr_digest_value{algorithm, std::vector<std::uint8_t>{data, data + size}};
}

[[nodiscard]] outcome<pcr_digest_value> make_digest_value(const TPMT_HA& digest)
{
    auto algorithm = algorithm_from_tpm(digest.hashAlg);
    if (!algorithm.has_value()) {
        return tl::unexpected(algorithm.error());
    }

    switch (algorithm.value()) {
    case hash_algorithm::sha1:
        return make_digest_value(algorithm.value(), digest.digest.sha1, TPM2_SHA1_DIGEST_SIZE);
    case hash_algorithm::sha256:
        return make_digest_value(algorithm.value(), digest.digest.sha256, TPM2_SHA256_DIGEST_SIZE);
    case hash_algorithm::sha384:
        return make_digest_value(algorithm.value(), digest.digest.sha384, TPM2_SHA384_DIGEST_SIZE);
    case hash_algorithm::sha512:
        return make_digest_value(algorithm.value(), digest.digest.sha512, TPM2_SHA512_DIGEST_SIZE);
    }

    return tl::unexpected(
        error{error_category::backend_error, "TPM returned unsupported PCR hash algorithm"});
}

void set_digest_bytes(TPMT_HA& destination, const pcr_digest_value& source) noexcept
{
    destination.hashAlg = algorithm_to_tpm(source.algorithm());
    const auto& bytes = source.digest();

    switch (source.algorithm()) {
    case hash_algorithm::sha1:
        std::copy(bytes.begin(), bytes.end(), destination.digest.sha1);
        return;
    case hash_algorithm::sha256:
        std::copy(bytes.begin(), bytes.end(), destination.digest.sha256);
        return;
    case hash_algorithm::sha384:
        std::copy(bytes.begin(), bytes.end(), destination.digest.sha384);
        return;
    case hash_algorithm::sha512:
        std::copy(bytes.begin(), bytes.end(), destination.digest.sha512);
        return;
    }
}

[[nodiscard]] TPML_DIGEST_VALUES
to_tpm_digest_values(const gsl::span<const pcr_digest_value> digests)
{
    TPML_DIGEST_VALUES result{};
    result.count = static_cast<UINT32>(digests.size());

    for (std::size_t index = 0U; index < digests.size(); ++index) {
        set_digest_bytes(result.digests[index], digests[index]);
    }

    return result;
}

[[nodiscard]] TPML_PCR_SELECTION to_tpm_selection(const pcr_selection& selection) noexcept
{
    TPML_PCR_SELECTION result{};
    result.count = 1U;
    TPMS_PCR_SELECTION& tpm_selection = result.pcrSelections[0U];
    tpm_selection.hash = algorithm_to_tpm(selection.algorithm());
    tpm_selection.sizeofSelect = pcr_select_min_size;

    for (const pcr_index index : selection.indices()) {
        const std::uint8_t value = index.value();
        tpm_selection.sizeofSelect =
            std::max(tpm_selection.sizeofSelect, selection_size_for_pcr(value));
        tpm_selection.pcrSelect[value / 8U] =
            static_cast<BYTE>(tpm_selection.pcrSelect[value / 8U] | (1U << (value % 8U)));
    }

    return result;
}

void select_all_pcrs(TPMS_PCR_SELECTION& selection) noexcept
{
    selection.sizeofSelect = pcr_select_min_size;
    for (std::size_t index = 0U; index < pcr_select_min_size; ++index) {
        selection.pcrSelect[index] = 0xffU;
    }
}

[[nodiscard]] TPML_PCR_SELECTION to_tpm_allocation(const gsl::span<const pcr_bank> banks) noexcept
{
    TPML_PCR_SELECTION result{};
    result.count = static_cast<UINT32>(banks.size());

    for (std::size_t index = 0U; index < banks.size(); ++index) {
        TPMS_PCR_SELECTION& selection = result.pcrSelections[index];
        selection.hash = algorithm_to_tpm(banks[index].algorithm());
        select_all_pcrs(selection);
    }

    return result;
}

[[nodiscard]] outcome<pcr_selection> to_domain_selection(const TPML_PCR_SELECTION& selection,
                                                         const hash_algorithm fallback_algorithm)
{
    if (selection.count == 0U) {
        return pcr_selection{fallback_algorithm};
    }

    if (selection.count != 1U) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned multiple PCR selections"});
    }

    const TPMS_PCR_SELECTION& tpm_selection = selection.pcrSelections[0U];
    auto algorithm = algorithm_from_tpm(tpm_selection.hash);
    if (!algorithm.has_value()) {
        return tl::unexpected(algorithm.error());
    }

    std::set<pcr_index> indices;
    if (tpm_selection.sizeofSelect > TPM2_PCR_SELECT_MAX) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned malformed PCR selection size"});
    }

    const std::size_t select_size = tpm_selection.sizeofSelect;
    for (std::size_t byte_index = 0U; byte_index < select_size; ++byte_index) {
        for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
            if ((tpm_selection.pcrSelect[byte_index] & (1U << bit)) == 0U) {
                continue;
            }

            const std::size_t pcr = (byte_index * 8U) + bit;
            if (pcr > pcr_index::max_value) {
                return tl::unexpected(
                    error{error_category::backend_error, "TPM returned invalid PCR index"});
            }

            indices.insert(pcr_index{static_cast<std::uint32_t>(pcr)});
        }
    }

    return pcr_selection{algorithm.value(), std::move(indices)};
}

[[nodiscard]] outcome<std::vector<pcr_digest_value>>
to_domain_digests(const TPML_DIGEST_VALUES& digests)
{
    if (digests.count > array_size(digests.digests)) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned too many PCR event digests"});
    }

    std::vector<pcr_digest_value> result;
    result.reserve(digests.count);

    for (UINT32 index = 0U; index < digests.count; ++index) {
        auto value = make_digest_value(digests.digests[index]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        result.push_back(std::move(value.value()));
    }

    return result;
}

[[nodiscard]] outcome<std::vector<pcr_digest_value>>
to_domain_read_values(const TPML_DIGEST& values, const hash_algorithm algorithm)
{
    if (values.count > array_size(values.digests)) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned too many PCR read digests"});
    }

    std::vector<pcr_digest_value> result;
    result.reserve(values.count);

    for (UINT32 index = 0U; index < values.count; ++index) {
        const TPM2B_DIGEST& digest = values.digests[index];
        auto value = make_digest_value(algorithm, digest.buffer, digest.size);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        result.push_back(std::move(value.value()));
    }

    return result;
}

[[nodiscard]] ESYS_TR pcr_handle(const pcr_index index) noexcept
{
    return ESYS_TR_PCR0 + index.value();
}

[[nodiscard]] TPM2B_EVENT to_tpm_event(const gsl::span<const std::uint8_t> event_data) noexcept
{
    TPM2B_EVENT event{};
    event.size = static_cast<UINT16>(event_data.size());
    if (!event_data.empty()) {
        std::memcpy(event.buffer, event_data.data(), event_data.size());
    }

    return event;
}

[[nodiscard]] TPM2B_DIGEST to_tpm_sized_digest(const gsl::span<const std::uint8_t> bytes) noexcept
{
    TPM2B_DIGEST result{};
    result.size = static_cast<UINT16>(bytes.size());
    if (!bytes.empty()) {
        std::memcpy(result.buffer, bytes.data(), bytes.size());
    }

    return result;
}

void log_read_completed(logger* const log, const pcr_selection& selection,
                        const std::size_t value_count)
{
    if (log == nullptr) {
        return;
    }

    const std::string pcr_count = std::to_string(value_count);
    const std::string bank = algorithm_name(selection.algorithm());
    const std::array<log_field, 2U> fields{{
        {events::fields::bank, bank},
        {events::fields::pcr_count, pcr_count},
    }};

    log->log(log_level::info, events::pcr_read_completed, gsl::span<const log_field>(fields));
}

void log_extend_completed(logger* const log, const pcr_index index, const std::size_t bank_count)
{
    if (log == nullptr) {
        return;
    }

    const std::string index_value = std::to_string(index.value());
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::bank_count, bank_count_value},
        {events::fields::pcr_index, index_value},
    }};

    log->log(log_level::info, events::pcr_extend_completed, gsl::span<const log_field>(fields));
}

void log_event_completed(logger* const log, const pcr_index index, const std::size_t event_size)
{
    if (log == nullptr) {
        return;
    }

    const std::string index_value = std::to_string(index.value());
    const std::string event_size_value = std::to_string(event_size);
    const std::array<log_field, 2U> fields{{
        {events::fields::event_size, event_size_value},
        {events::fields::pcr_index, index_value},
    }};

    log->log(log_level::info, events::pcr_event_completed, gsl::span<const log_field>(fields));
}

void log_allocate_completed(logger* const log, const bool allocation_success,
                            const std::size_t bank_count)
{
    if (log == nullptr) {
        return;
    }

    const std::string allocation_success_value = allocation_success ? "true" : "false";
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::allocation_success, allocation_success_value},
        {events::fields::bank_count, bank_count_value},
    }};

    log->log(log_level::info, events::pcr_allocate_completed, gsl::span<const log_field>(fields));
}

void log_auth_policy_set(logger* const log, const pcr_index index, const hash_algorithm algorithm)
{
    if (log == nullptr) {
        return;
    }

    const std::string index_value = std::to_string(index.value());
    const std::string algorithm_value = algorithm_name(algorithm);
    const std::array<log_field, 2U> fields{{
        {events::fields::pcr_index, index_value},
        {events::fields::policy_algorithm, algorithm_value},
    }};

    log->log(log_level::info, events::pcr_auth_policy_set, gsl::span<const log_field>(fields));
}

void log_auth_value_set(logger* const log, const pcr_index index)
{
    if (log == nullptr) {
        return;
    }

    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log->log(log_level::info, events::pcr_auth_value_set, gsl::span<const log_field>(fields));
}

void log_reset_completed(logger* const log, const pcr_index index)
{
    if (log == nullptr) {
        return;
    }

    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log->log(log_level::info, events::pcr_reset_completed, gsl::span<const log_field>(fields));
}

} // namespace

esys_pcr_provider::esys_pcr_provider(ESYS_CONTEXT* const esys, logger* const log,
                                     pcr_observer* const observer) noexcept
    : esys_{esys}, log_{log}, observer_{observer}
{}

outcome<pcr_allocate_result> esys_pcr_provider::allocate(const gsl::span<const pcr_bank> banks)
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
    const TSS2_RC rc = Esys_PCR_Allocate(esys_, ESYS_TR_RH_PLATFORM, ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                         ESYS_TR_NONE, &allocation, &allocation_success, &max_pcr,
                                         &size_needed, &size_available);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_allocate", log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    pcr_allocate_result result{allocation_success != TPM2_NO, max_pcr, size_needed, size_available};
    log_allocate_completed(log_, result.allocation_success, banks.size());
    return result;
}

outcome<pcr_event_result> esys_pcr_provider::event(const pcr_index index,
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
    const TSS2_RC rc = Esys_PCR_Event(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                      ESYS_TR_NONE, &tpm_event, &raw_digests);
    unique_digest_values digests{raw_digests, &Esys_Free};
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_event", log_, events::pcr_tss_error);
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

    pcr_event_result result{std::move(domain_digests.value())};
    log_event_completed(log_, index, event_data.size());
    if (observer_ != nullptr) {
        observer_->on_event(index, event_data, result);
    }

    return result;
}

outcome<void> esys_pcr_provider::extend(const pcr_index index,
                                        const gsl::span<const pcr_digest_value> digests)
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
    const TSS2_RC rc = Esys_PCR_Extend(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE,
                                       ESYS_TR_NONE, &tpm_digests);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_extend", log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    log_extend_completed(log_, index, digests.size());
    if (observer_ != nullptr) {
        observer_->on_extend(index, digests);
    }

    return {};
}

outcome<pcr_read_result> esys_pcr_provider::read(const pcr_selection& selection)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const TPML_PCR_SELECTION tpm_selection = to_tpm_selection(selection);
    UINT32 update_counter = 0U;
    TPML_PCR_SELECTION* raw_selection = nullptr;
    TPML_DIGEST* raw_values = nullptr;
    const TSS2_RC rc = Esys_PCR_Read(esys_, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                                     &tpm_selection, &update_counter, &raw_selection, &raw_values);
    unique_pcr_selection actual_selection{raw_selection, &Esys_Free};
    unique_pcr_values values{raw_values, &Esys_Free};
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_read", log_, events::pcr_tss_error);
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

    auto domain_values = to_domain_read_values(*values, domain_selection.value().algorithm());
    if (!domain_values.has_value()) {
        return tl::unexpected(domain_values.error());
    }

    pcr_read_result result{domain_selection.value(), update_counter,
                           std::move(domain_values.value())};
    log_read_completed(log_, result.actual_selection, result.values.size());
    return result;
}

outcome<void> esys_pcr_provider::reset(const pcr_index index)
{
    const auto available = ensure_esys_available(esys_);
    if (!available.has_value()) {
        return tl::unexpected(available.error());
    }

    const TSS2_RC rc =
        Esys_PCR_Reset(esys_, pcr_handle(index), ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_reset", log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    log_reset_completed(log_, index);
    return {};
}

outcome<void> esys_pcr_provider::set_auth_policy(const pcr_index index,
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
    const TSS2_RC rc = Esys_PCR_SetAuthPolicy(
        esys_, ESYS_TR_RH_PLATFORM, ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, &auth_policy,
        algorithm_to_tpm(policy_alg), static_cast<TPMI_DH_PCR>(TPM2_HR_PCR + index.value()));
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_set_auth_policy", log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    log_auth_policy_set(log_, index, policy_alg);
    return {};
}

outcome<void> esys_pcr_provider::set_auth_value(const pcr_index index, secret_buffer auth)
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
    const TSS2_RC set_auth_rc = Esys_TR_SetAuth(esys_, pcr_handle(index), &current_auth);
    if (set_auth_rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(set_auth_rc, "pcr_set_auth_value_set_current_auth", log_,
                                           events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    const TPM2B_DIGEST tpm_auth = to_tpm_sized_digest(auth_view);
    const TSS2_RC rc = Esys_PCR_SetAuthValue(esys_, pcr_handle(index), ESYS_TR_PASSWORD,
                                             ESYS_TR_NONE, ESYS_TR_NONE, &tpm_auth);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(rc, "pcr_set_auth_value", log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    const TPM2B_AUTH new_auth{};
    const TSS2_RC update_auth_rc = Esys_TR_SetAuth(esys_, pcr_handle(index), &new_auth);
    if (update_auth_rc != TSS2_RC_SUCCESS) {
        auto translated = translate_tss_rc(update_auth_rc, "pcr_set_auth_value_update_current_auth",
                                           log_, events::pcr_tss_error);
        return tl::unexpected(translated.error());
    }

    log_auth_value_set(log_, index);
    return {};
}

} // namespace tpmkit::detail::esys
