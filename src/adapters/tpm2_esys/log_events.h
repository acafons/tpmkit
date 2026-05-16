#pragma once

#include <string_view>

namespace tpmkit::detail::esys::events {

inline constexpr std::string_view esys_initialized = "tpm.context.esys_initialized";
inline constexpr std::string_view finalized = "tpm.context.finalized";
inline constexpr std::string_view startup_completed = "tpm.context.startup_completed";
inline constexpr std::string_view startup_invoked = "tpm.context.startup_invoked";
inline constexpr std::string_view tcti_configured = "tpm.context.tcti_configured";
inline constexpr std::string_view tcti_configuring = "tpm.context.tcti_configuring";
inline constexpr std::string_view tss_error = "tpm.context.tss_error";

namespace fields {

inline constexpr std::string_view abi_version = "abi_version";
inline constexpr std::string_view operation = "operation";
inline constexpr std::string_view result = "result";
inline constexpr std::string_view startup_mode = "startup_mode";
inline constexpr std::string_view tcti_kind = "tcti_kind";
inline constexpr std::string_view tcti_name = "tcti_name";
inline constexpr std::string_view tss_rc_hex = "tss_rc_hex";
inline constexpr std::string_view tss_layer = "tss_layer";

} // namespace fields

} // namespace tpmkit::detail::esys::events
