#pragma once

#include <tpmkit/result.h>

#include <tss2/tss2_common.h>

#include <string_view>

namespace tpmkit {

class logger;

namespace detail::esys {

[[nodiscard]] outcome<void> translate_tss_rc(TSS2_RC rc, std::string_view operation, logger* log);

[[nodiscard]] outcome<void> translate_tss_rc(TSS2_RC rc, std::string_view operation, logger* log,
                                             std::string_view error_event);

} // namespace detail::esys
} // namespace tpmkit
