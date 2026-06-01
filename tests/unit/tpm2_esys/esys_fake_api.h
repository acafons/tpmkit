#pragma once

#if defined(__GNUC__) || defined(__clang__)
// TSS constants in public headers intentionally expand through C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/testing/recording_logger.h>
#include <tpmkit/tpm2_esys/owned_tcti_context.h>

#include "src/adapters/tpm2_esys/pcr/pcr_marshalling.h"
#include "src/adapters/tpm2_esys/support/esys_api.h"

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tpmkit::testing::esys {

struct allocate_call {
    ESYS_TR auth_handle{};
    TPML_PCR_SELECTION allocation{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct event_call {
    TPM2B_EVENT event_data{};
    ESYS_TR pcr_handle{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct extend_call {
    TPML_DIGEST_VALUES digests{};
    ESYS_TR pcr_handle{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct read_call {
    TPML_PCR_SELECTION selection{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct reset_call {
    ESYS_TR pcr_handle{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct set_auth_policy_call {
    ESYS_TR auth_handle{};
    TPM2B_DIGEST auth_policy{};
    TPMI_ALG_HASH hash_alg{};
    TPMI_DH_PCR pcr_num{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct set_auth_value_call {
    TPM2B_DIGEST auth{};
    ESYS_TR pcr_handle{};
    ESYS_TR shandle1{};
    ESYS_TR shandle2{};
    ESYS_TR shandle3{};
};

struct tr_set_auth_call {
    TPM2B_AUTH auth{};
    ESYS_TR handle{};
};

struct fake_esys_state {
    TSS2_ABI_VERSION abi_version{1U, 2U, 3U, 4U};
    TPMI_YES_NO allocation_success{TPM2_YES};
    UINT32 max_pcr{32U};
    TSS2_RC pcr_allocate_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_event_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_extend_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_read_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_reset_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_set_auth_policy_rc{TSS2_RC_SUCCESS};
    TSS2_RC pcr_set_auth_value_rc{TSS2_RC_SUCCESS};
    TSS2_RC initialize_rc{TSS2_RC_SUCCESS};
    TSS2_RC startup_rc{TSS2_RC_SUCCESS};
    TSS2_RC tr_set_auth_rc{TSS2_RC_SUCCESS};
    TPML_PCR_SELECTION read_actual_selection{};
    TPML_DIGEST read_values{};
    UINT32 read_update_counter{0U};
    TPML_DIGEST_VALUES event_digests{};
    UINT32 size_available{44U};
    UINT32 size_needed{12U};
    TPM2_SU startup_type{};
    TSS2_TCTI_CONTEXT* initialized_tcti{};
    std::size_t esys_finalizes{0U};
    std::size_t initializes{0U};
    std::size_t startups{0U};
    std::size_t tcti_finalizes{0U};
    std::vector<allocate_call> allocate_calls;
    std::vector<event_call> event_calls;
    std::vector<extend_call> extend_calls;
    std::vector<read_call> read_calls;
    std::vector<reset_call> reset_calls;
    std::vector<set_auth_policy_call> set_auth_policy_calls;
    std::vector<set_auth_value_call> set_auth_value_calls;
    std::vector<tr_set_auth_call> tr_set_auth_calls;
};

template <class T> T* copy_to_heap(const T& value)
{
    void* const storage = std::malloc(sizeof(T));
    if (storage == nullptr) {
        return nullptr;
    }

    std::memcpy(storage, &value, sizeof(T));
    return static_cast<T*>(storage);
}

inline ESYS_CONTEXT* esys_handle(fake_esys_state& state) noexcept
{
    return reinterpret_cast<ESYS_CONTEXT*>(&state);
}

inline TSS2_TCTI_CONTEXT* tcti_handle(fake_esys_state& state) noexcept
{
    return reinterpret_cast<TSS2_TCTI_CONTEXT*>(&state);
}

inline fake_esys_state& state_from(ESYS_CONTEXT* const context) noexcept
{
    return *reinterpret_cast<fake_esys_state*>(context);
}

inline fake_esys_state& state_from(TSS2_TCTI_CONTEXT* const context) noexcept
{
    return *reinterpret_cast<fake_esys_state*>(context);
}

inline void finalize_tcti_handle(TSS2_TCTI_CONTEXT* const context) noexcept
{
    if (context != nullptr) {
        ++state_from(context).tcti_finalizes;
    }
}

inline tpm2_esys::owned_tcti_context owned_tcti(fake_esys_state& state)
{
    return tpm2_esys::owned_tcti_context{
        std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(tcti_handle(state),
                                                                         finalize_tcti_handle)};
}

inline void fake_free(void* const ptr) noexcept
{
    std::free(ptr);
}

inline TSS2_RC fake_initialize(ESYS_CONTEXT** const context, TSS2_TCTI_CONTEXT* const tcti,
                               TSS2_ABI_VERSION* const abi_version)
{
    fake_esys_state& state = state_from(tcti);
    ++state.initializes;
    state.initialized_tcti = tcti;
    if (abi_version != nullptr) {
        *abi_version = state.abi_version;
    }
    if (state.initialize_rc == TSS2_RC_SUCCESS && context != nullptr) {
        *context = esys_handle(state);
    }
    return state.initialize_rc;
}

inline void fake_finalize(ESYS_CONTEXT** const context) noexcept
{
    if (context == nullptr || *context == nullptr) {
        return;
    }

    ++state_from(*context).esys_finalizes;
    *context = nullptr;
}

inline TSS2_RC fake_pcr_allocate(ESYS_CONTEXT* const context, const ESYS_TR auth_handle,
                                 const ESYS_TR shandle1, const ESYS_TR shandle2,
                                 const ESYS_TR shandle3, const TPML_PCR_SELECTION* const allocation,
                                 TPMI_YES_NO* const allocation_success, UINT32* const max_pcr,
                                 UINT32* const size_needed, UINT32* const size_available)
{
    fake_esys_state& state = state_from(context);
    state.allocate_calls.push_back(
        allocate_call{auth_handle, allocation == nullptr ? TPML_PCR_SELECTION{} : *allocation,
                      shandle1, shandle2, shandle3});
    if (state.pcr_allocate_rc != TSS2_RC_SUCCESS) {
        return state.pcr_allocate_rc;
    }

    *allocation_success = state.allocation_success;
    *max_pcr = state.max_pcr;
    *size_needed = state.size_needed;
    *size_available = state.size_available;
    return TSS2_RC_SUCCESS;
}

inline TSS2_RC fake_pcr_event(ESYS_CONTEXT* const context, const ESYS_TR pcr_handle,
                              const ESYS_TR shandle1, const ESYS_TR shandle2,
                              const ESYS_TR shandle3, const TPM2B_EVENT* const event_data,
                              TPML_DIGEST_VALUES** const digests)
{
    fake_esys_state& state = state_from(context);
    state.event_calls.push_back(event_call{event_data == nullptr ? TPM2B_EVENT{} : *event_data,
                                           pcr_handle, shandle1, shandle2, shandle3});
    if (state.pcr_event_rc != TSS2_RC_SUCCESS) {
        return state.pcr_event_rc;
    }

    *digests = copy_to_heap(state.event_digests);
    return TSS2_RC_SUCCESS;
}

inline TSS2_RC fake_pcr_extend(ESYS_CONTEXT* const context, const ESYS_TR pcr_handle,
                               const ESYS_TR shandle1, const ESYS_TR shandle2,
                               const ESYS_TR shandle3, const TPML_DIGEST_VALUES* const digests)
{
    fake_esys_state& state = state_from(context);
    state.extend_calls.push_back(extend_call{digests == nullptr ? TPML_DIGEST_VALUES{} : *digests,
                                             pcr_handle, shandle1, shandle2, shandle3});
    return state.pcr_extend_rc;
}

inline TSS2_RC fake_pcr_read(ESYS_CONTEXT* const context, const ESYS_TR shandle1,
                             const ESYS_TR shandle2, const ESYS_TR shandle3,
                             const TPML_PCR_SELECTION* const selection,
                             UINT32* const update_counter,
                             TPML_PCR_SELECTION** const actual_selection,
                             TPML_DIGEST** const values)
{
    fake_esys_state& state = state_from(context);
    state.read_calls.push_back(read_call{selection == nullptr ? TPML_PCR_SELECTION{} : *selection,
                                         shandle1, shandle2, shandle3});
    if (state.pcr_read_rc != TSS2_RC_SUCCESS) {
        return state.pcr_read_rc;
    }

    *update_counter = state.read_update_counter;
    *actual_selection = copy_to_heap(state.read_actual_selection);
    *values = copy_to_heap(state.read_values);
    return TSS2_RC_SUCCESS;
}

inline TSS2_RC fake_pcr_reset(ESYS_CONTEXT* const context, const ESYS_TR pcr_handle,
                              const ESYS_TR shandle1, const ESYS_TR shandle2,
                              const ESYS_TR shandle3)
{
    fake_esys_state& state = state_from(context);
    state.reset_calls.push_back(reset_call{pcr_handle, shandle1, shandle2, shandle3});
    return state.pcr_reset_rc;
}

inline TSS2_RC fake_pcr_set_auth_policy(ESYS_CONTEXT* const context, const ESYS_TR auth_handle,
                                        const ESYS_TR shandle1, const ESYS_TR shandle2,
                                        const ESYS_TR shandle3,
                                        const TPM2B_DIGEST* const auth_policy,
                                        const TPMI_ALG_HASH hash_alg, const TPMI_DH_PCR pcr_num)
{
    fake_esys_state& state = state_from(context);
    state.set_auth_policy_calls.push_back(
        set_auth_policy_call{auth_handle, auth_policy == nullptr ? TPM2B_DIGEST{} : *auth_policy,
                             hash_alg, pcr_num, shandle1, shandle2, shandle3});
    return state.pcr_set_auth_policy_rc;
}

inline TSS2_RC fake_pcr_set_auth_value(ESYS_CONTEXT* const context, const ESYS_TR pcr_handle,
                                       const ESYS_TR shandle1, const ESYS_TR shandle2,
                                       const ESYS_TR shandle3, const TPM2B_DIGEST* const auth)
{
    fake_esys_state& state = state_from(context);
    state.set_auth_value_calls.push_back(set_auth_value_call{
        auth == nullptr ? TPM2B_DIGEST{} : *auth, pcr_handle, shandle1, shandle2, shandle3});
    return state.pcr_set_auth_value_rc;
}

inline TSS2_RC fake_startup(ESYS_CONTEXT* const context, const TPM2_SU startup_type)
{
    fake_esys_state& state = state_from(context);
    ++state.startups;
    state.startup_type = startup_type;
    return state.startup_rc;
}

inline TSS2_RC fake_tr_set_auth(ESYS_CONTEXT* const context, const ESYS_TR handle,
                                const TPM2B_AUTH* const auth)
{
    fake_esys_state& state = state_from(context);
    state.tr_set_auth_calls.push_back(
        tr_set_auth_call{auth == nullptr ? TPM2B_AUTH{} : *auth, handle});
    return state.tr_set_auth_rc;
}

inline const char* fake_decode_error(TSS2_RC)
{
    return "fake esys error";
}

inline const detail::tpm2_esys::esys_api& fake_api() noexcept
{
    static const detail::tpm2_esys::esys_api api{
        fake_initialize,          fake_finalize,           fake_free,     fake_pcr_allocate,
        fake_pcr_event,           fake_pcr_extend,         fake_pcr_read, fake_pcr_reset,
        fake_pcr_set_auth_policy, fake_pcr_set_auth_value, fake_startup,  fake_tr_set_auth,
        fake_decode_error,
    };

    return api;
}

inline std::vector<std::uint8_t> digest_bytes(const std::uint8_t seed)
{
    std::vector<std::uint8_t> bytes(TPM2_SHA256_DIGEST_SIZE);
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(seed + index);
    }

    return bytes;
}

inline tpmkit::pcr::digest_value sha256_digest(const std::uint8_t seed)
{
    return tpmkit::pcr::digest_value{tpmkit::hash_algorithm::sha256, digest_bytes(seed)};
}

inline TPML_PCR_SELECTION sha256_selection(const std::uint8_t pcr)
{
    return detail::tpm2_esys::to_tpm_selection(
        tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256, {tpmkit::pcr::index{pcr}}});
}

inline TPML_DIGEST read_values(const std::vector<std::uint8_t>& digest)
{
    TPML_DIGEST values{};
    values.count = 1U;
    values.digests[0U].size = static_cast<UINT16>(digest.size());
    std::memcpy(values.digests[0U].buffer, digest.data(), digest.size());
    return values;
}

inline TPML_DIGEST_VALUES digest_values(const tpmkit::pcr::digest_value& digest)
{
    return detail::tpm2_esys::to_tpm_digest_values(gsl::make_span(&digest, 1U));
}

inline std::string field_value(const tpmkit::testing::log_record& record,
                               const std::string_view key)
{
    for (const auto& field : record.fields) {
        if (field.first == key) {
            return field.second;
        }
    }

    return {};
}

inline bool contains_field_value(const std::vector<tpmkit::testing::log_record>& records,
                                 const std::string_view forbidden)
{
    for (const auto& record : records) {
        for (const auto& field : record.fields) {
            if (field.second.find(forbidden) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

} // namespace tpmkit::testing::esys

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
