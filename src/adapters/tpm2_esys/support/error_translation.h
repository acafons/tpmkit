#pragma once

#include "log_events.h"
#include "tss_error_decoder.h"

#include <tpmkit/result.h>

#include <string_view>

namespace tpmkit {

class logger;

namespace detail::tpm2_esys {

[[nodiscard]] outcome<void> translate_tss_rc(TSS2_RC rc, std::string_view operation, logger* log);

[[nodiscard]] outcome<void> translate_tss_rc(TSS2_RC rc, std::string_view operation, logger* log,
                                             events::event_descriptor error_event);

[[nodiscard]] outcome<void> translate_tss_rc(TSS2_RC rc, std::string_view operation, logger* log,
                                             events::event_descriptor error_event,
                                             tss_error_decoder decoder);

} // namespace detail::tpm2_esys
} // namespace tpmkit
