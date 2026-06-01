#pragma once

#include "tss_error_decoder.h"

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

namespace tpmkit::detail::tpm2_esys {

struct esys_api {
    TSS2_RC (*initialize)(ESYS_CONTEXT** esys_context, TSS2_TCTI_CONTEXT* tcti,
                          TSS2_ABI_VERSION* abi_version);
    void (*finalize)(ESYS_CONTEXT** esys_context) noexcept;
    void (*free)(void* ptr) noexcept;
    TSS2_RC (*pcr_allocate)(ESYS_CONTEXT* esys_context, ESYS_TR auth_handle, ESYS_TR shandle1,
                            ESYS_TR shandle2, ESYS_TR shandle3,
                            const TPML_PCR_SELECTION* allocation, TPMI_YES_NO* allocation_success,
                            UINT32* max_pcr, UINT32* size_needed, UINT32* size_available);
    TSS2_RC (*pcr_event)(ESYS_CONTEXT* esys_context, ESYS_TR pcr_handle, ESYS_TR shandle1,
                         ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_EVENT* event_data,
                         TPML_DIGEST_VALUES** digests);
    TSS2_RC (*pcr_extend)(ESYS_CONTEXT* esys_context, ESYS_TR pcr_handle, ESYS_TR shandle1,
                          ESYS_TR shandle2, ESYS_TR shandle3, const TPML_DIGEST_VALUES* digests);
    TSS2_RC (*pcr_read)(ESYS_CONTEXT* esys_context, ESYS_TR shandle1, ESYS_TR shandle2,
                        ESYS_TR shandle3, const TPML_PCR_SELECTION* pcr_selection_in,
                        UINT32* pcr_update_counter, TPML_PCR_SELECTION** pcr_selection_out,
                        TPML_DIGEST** pcr_values);
    TSS2_RC (*pcr_reset)(ESYS_CONTEXT* esys_context, ESYS_TR pcr_handle, ESYS_TR shandle1,
                         ESYS_TR shandle2, ESYS_TR shandle3);
    TSS2_RC (*pcr_set_auth_policy)(ESYS_CONTEXT* esys_context, ESYS_TR auth_handle,
                                   ESYS_TR shandle1, ESYS_TR shandle2, ESYS_TR shandle3,
                                   const TPM2B_DIGEST* auth_policy, TPMI_ALG_HASH hash_alg,
                                   TPMI_DH_PCR pcr_num);
    TSS2_RC (*pcr_set_auth_value)(ESYS_CONTEXT* esys_context, ESYS_TR pcr_handle, ESYS_TR shandle1,
                                  ESYS_TR shandle2, ESYS_TR shandle3, const TPM2B_DIGEST* auth);
    TSS2_RC (*startup)(ESYS_CONTEXT* esys_context, TPM2_SU startup_type);
    TSS2_RC (*tr_set_auth)(ESYS_CONTEXT* esys_context, ESYS_TR handle,
                           const TPM2B_AUTH* auth_value);
    tss_error_decoder decode_error;
};

struct esys_context_deleter {
    const esys_api* api{nullptr};

    void operator()(ESYS_CONTEXT* context) const noexcept;
};

[[nodiscard]] const esys_api& default_esys_api() noexcept;

} // namespace tpmkit::detail::tpm2_esys
