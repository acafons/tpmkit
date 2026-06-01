#pragma once

#include <tss2/tss2_common.h>

namespace tpmkit::detail::esys {

using tss_error_decoder = const char* (*)(TSS2_RC rc);

} // namespace tpmkit::detail::esys
