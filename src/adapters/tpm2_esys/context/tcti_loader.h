#pragma once

#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <memory>

extern "C" {
typedef struct TSS2_TCTI_OPAQUE_CONTEXT_BLOB TSS2_TCTI_CONTEXT;
}

namespace tpmkit {

class logger;

namespace detail::esys {

using unique_tcti_ptr = std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>;

[[nodiscard]] outcome<unique_tcti_ptr> load_tcti(const tcti_string_config& config, logger* log);

} // namespace detail::esys
} // namespace tpmkit
