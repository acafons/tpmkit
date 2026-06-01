#include "pcr_marshalling.h"

#include "../../../support/build_options.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

namespace tpmkit::detail::tpm2_esys {
namespace {

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

[[nodiscard]] outcome<pcr::digest_value>
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

    return pcr::digest_value{algorithm, std::vector<std::uint8_t>{data, data + size}};
}

[[nodiscard]] outcome<pcr::digest_value> make_digest_value(const TPMT_HA& digest)
{
    auto algorithm = algorithm_from_tpm(digest.hashAlg);
    if (!algorithm.has_value()) {
        return tl::unexpected(algorithm.error());
    }

    const hash_algorithm selected_algorithm = *algorithm;
    switch (selected_algorithm) {
    case hash_algorithm::sha1:
        return make_digest_value(selected_algorithm, digest.digest.sha1, TPM2_SHA1_DIGEST_SIZE);
    case hash_algorithm::sha256:
        return make_digest_value(selected_algorithm, digest.digest.sha256, TPM2_SHA256_DIGEST_SIZE);
    case hash_algorithm::sha384:
        return make_digest_value(selected_algorithm, digest.digest.sha384, TPM2_SHA384_DIGEST_SIZE);
    case hash_algorithm::sha512:
        return make_digest_value(selected_algorithm, digest.digest.sha512, TPM2_SHA512_DIGEST_SIZE);
    }

    return tl::unexpected(
        error{error_category::backend_error, "TPM returned unsupported PCR hash algorithm"});
}

[[nodiscard]] bool should_skip_disabled_pcr_algorithm(const TPMI_ALG_HASH algorithm) noexcept
{
    return !::tpmkit::detail::legacy_sha1_pcr_enabled && algorithm == TPM2_ALG_SHA1;
}

void set_digest_bytes(TPMT_HA& destination, const pcr::digest_value& source) noexcept
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

void select_all_pcrs(TPMS_PCR_SELECTION& selection) noexcept
{
    selection.sizeofSelect = pcr_select_min_size;
    for (std::size_t index = 0U; index < pcr_select_min_size; ++index) {
        selection.pcrSelect[index] = 0xffU;
    }
}

} // namespace

outcome<hash_algorithm> algorithm_from_tpm(const TPMI_ALG_HASH algorithm)
{
    switch (algorithm) {
    case TPM2_ALG_SHA1:
        if constexpr (::tpmkit::detail::legacy_sha1_pcr_enabled) {
            return hash_algorithm::sha1;
        } else {
            return tl::unexpected(error{error_category::backend_error,
                                        "TPM returned disabled legacy SHA-1 PCR bank"});
        }
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

TPMI_ALG_HASH algorithm_to_tpm(const hash_algorithm algorithm) noexcept
{
    switch (algorithm) {
    case hash_algorithm::sha1:
        if constexpr (::tpmkit::detail::legacy_sha1_pcr_enabled) {
            return TPM2_ALG_SHA1;
        } else {
            return TPM2_ALG_ERROR;
        }
    case hash_algorithm::sha256:
        return TPM2_ALG_SHA256;
    case hash_algorithm::sha384:
        return TPM2_ALG_SHA384;
    case hash_algorithm::sha512:
        return TPM2_ALG_SHA512;
    }

    return TPM2_ALG_ERROR;
}

bool is_supported_algorithm(const hash_algorithm algorithm) noexcept
{
    return algorithm_to_tpm(algorithm) != TPM2_ALG_ERROR;
}

ESYS_TR pcr_handle(const pcr::index index) noexcept
{
    return ESYS_TR_PCR0 + index.value();
}

outcome<std::vector<pcr::digest_value>> to_domain_digests(const TPML_DIGEST_VALUES& digests)
{
    if (digests.count > array_size(digests.digests)) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned too many PCR event digests"});
    }

    std::vector<pcr::digest_value> result;
    result.reserve(digests.count);

    for (UINT32 index = 0U; index < digests.count; ++index) {
        if (should_skip_disabled_pcr_algorithm(digests.digests[index].hashAlg)) {
            continue;
        }

        auto value = make_digest_value(digests.digests[index]);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        result.push_back(*std::move(value));
    }

    return result;
}

outcome<std::vector<pcr::value>> to_domain_read_values(const TPML_DIGEST& values,
                                                       const pcr::selection& selection)
{
    if (values.count > array_size(values.digests)) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned too many PCR read digests"});
    }

    if (values.count != selection.indices().size()) {
        return tl::unexpected(
            error{error_category::backend_error, "TPM returned PCR digest count mismatch"});
    }

    std::vector<pcr::value> result;
    result.reserve(values.count);

    auto selected_index = selection.indices().begin();
    for (UINT32 index = 0U; index < values.count; ++index) {
        const TPM2B_DIGEST& digest = values.digests[index];
        auto value = make_digest_value(selection.algorithm(), digest.buffer, digest.size);
        if (!value.has_value()) {
            return tl::unexpected(value.error());
        }

        result.push_back(pcr::value{*selected_index, *std::move(value)});
        ++selected_index;
    }

    return result;
}

outcome<pcr::selection> to_domain_selection(const TPML_PCR_SELECTION& selection,
                                            const hash_algorithm fallback_algorithm)
{
    if (selection.count == 0U) {
        return pcr::selection{fallback_algorithm};
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

    std::set<pcr::index> indices;
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

            const std::size_t pcr_number = (byte_index * 8U) + bit;
            if (pcr_number > pcr::index::max_value) {
                return tl::unexpected(
                    error{error_category::backend_error, "TPM returned invalid PCR index"});
            }

            indices.insert(pcr::index{static_cast<std::uint32_t>(pcr_number)});
        }
    }

    return pcr::selection{*algorithm, std::move(indices)};
}

TPML_PCR_SELECTION to_tpm_allocation(const gsl::span<const pcr::bank> banks) noexcept
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

TPML_DIGEST_VALUES to_tpm_digest_values(const gsl::span<const pcr::digest_value> digests)
{
    TPML_DIGEST_VALUES result{};
    result.count = static_cast<UINT32>(digests.size());

    for (std::size_t index = 0U; index < digests.size(); ++index) {
        set_digest_bytes(result.digests[index], digests[index]);
    }

    return result;
}

TPM2B_EVENT to_tpm_event(const gsl::span<const std::uint8_t> event_data) noexcept
{
    TPM2B_EVENT event{};
    event.size = static_cast<UINT16>(event_data.size());
    if (!event_data.empty()) {
        std::memcpy(event.buffer, event_data.data(), event_data.size());
    }

    return event;
}

TPML_PCR_SELECTION to_tpm_selection(const pcr::selection& selection) noexcept
{
    TPML_PCR_SELECTION result{};
    result.count = 1U;
    TPMS_PCR_SELECTION& tpm_selection = result.pcrSelections[0U];
    tpm_selection.hash = algorithm_to_tpm(selection.algorithm());
    tpm_selection.sizeofSelect = pcr_select_min_size;

    for (const pcr::index index : selection.indices()) {
        const std::uint8_t value = index.value();
        tpm_selection.sizeofSelect =
            std::max(tpm_selection.sizeofSelect, selection_size_for_pcr(value));
        tpm_selection.pcrSelect[value / 8U] =
            static_cast<BYTE>(tpm_selection.pcrSelect[value / 8U] | (1U << (value % 8U)));
    }

    return result;
}

TPM2B_DIGEST to_tpm_sized_digest(const gsl::span<const std::uint8_t> bytes) noexcept
{
    TPM2B_DIGEST result{};
    result.size = static_cast<UINT16>(bytes.size());
    if (!bytes.empty()) {
        std::memcpy(result.buffer, bytes.data(), bytes.size());
    }

    return result;
}

} // namespace tpmkit::detail::tpm2_esys
