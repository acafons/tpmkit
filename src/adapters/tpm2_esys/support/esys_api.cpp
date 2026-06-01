#include "esys_api.h"

#include <tss2/tss2_rc.h>

namespace tpmkit::detail::tpm2_esys {
namespace {

TSS2_RC initialize(ESYS_CONTEXT** const esys_context, TSS2_TCTI_CONTEXT* const tcti,
                   TSS2_ABI_VERSION* const abi_version)
{
    return Esys_Initialize(esys_context, tcti, abi_version);
}

void finalize(ESYS_CONTEXT** const esys_context) noexcept
{
    Esys_Finalize(esys_context);
}

void free_esys(void* const ptr) noexcept
{
    Esys_Free(ptr);
}

TSS2_RC pcr_allocate(ESYS_CONTEXT* const esys_context, const ESYS_TR auth_handle,
                     const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3,
                     const TPML_PCR_SELECTION* const allocation,
                     TPMI_YES_NO* const allocation_success, UINT32* const max_pcr,
                     UINT32* const size_needed, UINT32* const size_available)
{
    return Esys_PCR_Allocate(esys_context, auth_handle, shandle1, shandle2, shandle3, allocation,
                             allocation_success, max_pcr, size_needed, size_available);
}

TSS2_RC pcr_event(ESYS_CONTEXT* const esys_context, const ESYS_TR pcr_handle,
                  const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3,
                  const TPM2B_EVENT* const event_data, TPML_DIGEST_VALUES** const digests)
{
    return Esys_PCR_Event(esys_context, pcr_handle, shandle1, shandle2, shandle3, event_data,
                          digests);
}

TSS2_RC pcr_extend(ESYS_CONTEXT* const esys_context, const ESYS_TR pcr_handle,
                   const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3,
                   const TPML_DIGEST_VALUES* const digests)
{
    return Esys_PCR_Extend(esys_context, pcr_handle, shandle1, shandle2, shandle3, digests);
}

TSS2_RC pcr_read(ESYS_CONTEXT* const esys_context, const ESYS_TR shandle1, const ESYS_TR shandle2,
                 const ESYS_TR shandle3, const TPML_PCR_SELECTION* const pcr_selection_in,
                 UINT32* const pcr_update_counter, TPML_PCR_SELECTION** const pcr_selection_out,
                 TPML_DIGEST** const pcr_values)
{
    return Esys_PCR_Read(esys_context, shandle1, shandle2, shandle3, pcr_selection_in,
                         pcr_update_counter, pcr_selection_out, pcr_values);
}

TSS2_RC pcr_reset(ESYS_CONTEXT* const esys_context, const ESYS_TR pcr_handle,
                  const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3)
{
    return Esys_PCR_Reset(esys_context, pcr_handle, shandle1, shandle2, shandle3);
}

TSS2_RC pcr_set_auth_policy(ESYS_CONTEXT* const esys_context, const ESYS_TR auth_handle,
                            const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3,
                            const TPM2B_DIGEST* const auth_policy, const TPMI_ALG_HASH hash_alg,
                            const TPMI_DH_PCR pcr_num)
{
    return Esys_PCR_SetAuthPolicy(esys_context, auth_handle, shandle1, shandle2, shandle3,
                                  auth_policy, hash_alg, pcr_num);
}

TSS2_RC pcr_set_auth_value(ESYS_CONTEXT* const esys_context, const ESYS_TR pcr_handle,
                           const ESYS_TR shandle1, const ESYS_TR shandle2, const ESYS_TR shandle3,
                           const TPM2B_DIGEST* const auth)
{
    return Esys_PCR_SetAuthValue(esys_context, pcr_handle, shandle1, shandle2, shandle3, auth);
}

TSS2_RC startup(ESYS_CONTEXT* const esys_context, const TPM2_SU startup_type)
{
    return Esys_Startup(esys_context, startup_type);
}

TSS2_RC tr_set_auth(ESYS_CONTEXT* const esys_context, const ESYS_TR handle,
                    const TPM2B_AUTH* const auth_value)
{
    return Esys_TR_SetAuth(esys_context, handle, auth_value);
}

} // namespace

void esys_context_deleter::operator()(ESYS_CONTEXT* context) const noexcept
{
    if (context == nullptr || api == nullptr || api->finalize == nullptr) {
        return;
    }

    api->finalize(&context);
}

const esys_api& default_esys_api() noexcept
{
    static const esys_api api{
        initialize, finalize,    free_esys,       pcr_allocate,        pcr_event,
        pcr_extend, pcr_read,    pcr_reset,       pcr_set_auth_policy, pcr_set_auth_value,
        startup,    tr_set_auth, &Tss2_RC_Decode,
    };

    return api;
}

} // namespace tpmkit::detail::tpm2_esys
