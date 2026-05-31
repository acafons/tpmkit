#pragma once

namespace tpmkit::detail {

#if defined(TPMKIT_ENABLE_LEGACY_SHA1_PCR) && TPMKIT_ENABLE_LEGACY_SHA1_PCR
inline constexpr bool legacy_sha1_pcr_enabled = true;
#else
inline constexpr bool legacy_sha1_pcr_enabled = false;
#endif

} // namespace tpmkit::detail
