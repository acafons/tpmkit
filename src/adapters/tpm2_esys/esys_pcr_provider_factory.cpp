#include <tpmkit/esys_pcr_provider.h>

#include "esys_pcr_provider.h"
#include "impl.h"

#include <tpmkit/logging/noop_logger.h>

#include <tss2/tss2_esys.h>

#include <memory>

namespace tpmkit {
namespace {

[[nodiscard]] logger* effective_logger(logger* const log) noexcept
{
    if (log != nullptr) {
        return log;
    }

    static noop_logger default_logger;
    return &default_logger;
}

[[nodiscard]] error invalid_context_error()
{
    return error{error_category::resource_error,
                 "TPM context does not contain a usable ESYS handle"};
}

} // namespace

outcome<std::unique_ptr<pcr_provider>>
create_esys_pcr_provider(tpm_context& ctx, pcr_observer* const observer, logger* const log)
{
    auto* const esys = static_cast<ESYS_CONTEXT*>(ctx.esys_handle());
    if (esys == nullptr) {
        return tl::unexpected(invalid_context_error());
    }

    return std::make_unique<detail::esys::esys_pcr_provider>(esys, effective_logger(log), observer);
}

} // namespace tpmkit
