#pragma once

#include <string_view>

namespace tpmkit::detail::esys::events {

inline constexpr std::string_view esys_initialized = "tpm.context.esys_initialized";
inline constexpr std::string_view finalized = "tpm.context.finalized";
inline constexpr std::string_view pcr_allocate_completed = "tpm.pcr.allocate_completed";
inline constexpr std::string_view pcr_auth_policy_set = "tpm.pcr.auth_policy_set";
inline constexpr std::string_view pcr_auth_value_set = "tpm.pcr.auth_value_set";
inline constexpr std::string_view pcr_event_completed = "tpm.pcr.event_completed";
inline constexpr std::string_view pcr_extend_completed = "tpm.pcr.extend_completed";
inline constexpr std::string_view pcr_read_completed = "tpm.pcr.read_completed";
inline constexpr std::string_view pcr_reset_completed = "tpm.pcr.reset_completed";
inline constexpr std::string_view pcr_tss_error = "tpm.pcr.tss_error";
inline constexpr std::string_view startup_completed = "tpm.context.startup_completed";
inline constexpr std::string_view startup_invoked = "tpm.context.startup_invoked";
inline constexpr std::string_view tcti_configured = "tpm.context.tcti_configured";
inline constexpr std::string_view tcti_configuring = "tpm.context.tcti_configuring";
inline constexpr std::string_view tss_error = "tpm.context.tss_error";

namespace fields {

inline constexpr std::string_view abi_version = "abi_version";
inline constexpr std::string_view allocation_success = "allocation_success";
inline constexpr std::string_view bank = "bank";
inline constexpr std::string_view bank_count = "bank_count";
inline constexpr std::string_view event_size = "event_size";
inline constexpr std::string_view operation = "operation";
inline constexpr std::string_view policy_algorithm = "policy_algorithm";
inline constexpr std::string_view pcr_count = "pcr_count";
inline constexpr std::string_view pcr_index = "pcr_index";
inline constexpr std::string_view result = "result";
inline constexpr std::string_view startup_mode = "startup_mode";
inline constexpr std::string_view tcti_kind = "tcti_kind";
inline constexpr std::string_view tcti_name = "tcti_name";
inline constexpr std::string_view tss_rc_hex = "tss_rc_hex";
inline constexpr std::string_view tss_layer = "tss_layer";

} // namespace fields

} // namespace tpmkit::detail::esys::events
