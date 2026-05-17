#pragma once

#include "tcti_loader.h"

#include <tpmkit/tpm_context.h>

#include <tss2/tss2_esys.h>

#include <memory>
#include <string_view>

namespace tpmkit {

namespace detail::esys {

using unique_esys_ptr = std::unique_ptr<ESYS_CONTEXT, void (*)(ESYS_CONTEXT*)>;

[[nodiscard]] bool is_startup_already_initialized(TSS2_RC rc) noexcept;

[[nodiscard]] std::string_view startup_result_field(TSS2_RC rc) noexcept;

} // namespace detail::esys

class tpm_context::impl final {
public:
    impl(detail::esys::unique_tcti_ptr tcti, detail::esys::unique_esys_ptr esys,
         std::shared_ptr<logger> log) noexcept;

    ~impl() noexcept;

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    [[nodiscard]] ESYS_CONTEXT* esys() const noexcept;

private:
    detail::esys::unique_tcti_ptr tcti_;
    detail::esys::unique_esys_ptr esys_;
    std::shared_ptr<logger> log_;
};

} // namespace tpmkit
