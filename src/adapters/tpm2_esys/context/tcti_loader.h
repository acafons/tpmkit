#pragma once

#include "../support/tss_error_decoder.h"

#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <tss2/tss2_common.h>

#include <memory>

extern "C" {
typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;
typedef struct TSS2_TCTI_INFO TSS2_TCTI_INFO;
}

namespace tpmkit {

class logger;

namespace detail::esys {

using unique_tcti_ptr = std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>;

struct tcti_loader_api {
    TSS2_RC (*get_info)(const char* name, TSS2_TCTI_INFO** info);
    void (*free_info)(TSS2_TCTI_INFO** info) noexcept;
    TSS2_RC (*init)(TSS2_TCTI_CONTEXT* context, std::size_t* size, const char* conf);
    tss_error_decoder decode_error;
};

[[nodiscard]] outcome<unique_tcti_ptr> load_tcti(const tcti_string_config& config, logger* log);

[[nodiscard]] outcome<unique_tcti_ptr> load_tcti(const tcti_string_config& config, logger* log,
                                                 const tcti_loader_api& api);

[[nodiscard]] const tcti_loader_api& default_tcti_loader_api() noexcept;

} // namespace detail::esys
} // namespace tpmkit
