#pragma once

#include "../support/esys_api.h"
#include "tcti_loader.h"

#include <tpmkit/tpm2_esys/owned_tcti_context.h>
#include <tpmkit/tpm_context.h>

#include <tss2/tss2_esys.h>

#include <memory>
#include <string_view>

namespace tpmkit {

namespace detail::tpm2_esys {

using unique_esys_ptr = std::unique_ptr<ESYS_CONTEXT, esys_context_deleter>;

[[nodiscard]] bool is_startup_already_initialized(TSS2_RC rc) noexcept;

[[nodiscard]] std::string_view startup_result_field(TSS2_RC rc) noexcept;

[[nodiscard]] outcome<tpm_context>
create_context_from_owned_tcti(::tpmkit::tpm2_esys::owned_tcti_context tcti,
                               tpm_context_config::startup_mode startup,
                               std::shared_ptr<logger> log, const esys_api& api);

[[nodiscard]] outcome<tpm_context> create_context_from_config(tpm_context_config config,
                                                              const tcti_loader_api& tcti_api,
                                                              const esys_api& api);

} // namespace detail::tpm2_esys

class tpm_context::impl final {
public:
    impl(detail::tpm2_esys::unique_tcti_ptr tcti, detail::tpm2_esys::unique_esys_ptr esys,
         std::shared_ptr<logger> log, const detail::tpm2_esys::esys_api& api) noexcept;

    ~impl() noexcept;

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    [[nodiscard]] ESYS_CONTEXT* esys() const noexcept;
    [[nodiscard]] const detail::tpm2_esys::esys_api& esys_api() const noexcept;
    [[nodiscard]] logger& log() const noexcept;

private:
    // Non-alphabetical order is intentional: C++ destroys in reverse declaration
    // order, so esys_ is finalised before tcti_ — matching the TSS teardown contract.
    detail::tpm2_esys::unique_tcti_ptr tcti_;
    detail::tpm2_esys::unique_esys_ptr esys_;
    std::shared_ptr<logger> log_;
    const detail::tpm2_esys::esys_api& api_;
};

} // namespace tpmkit
