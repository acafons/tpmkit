#pragma once

#include <string_view>

namespace tpmkit::detail::esys::events {

struct event_descriptor {
    std::string_view name;
    std::string_view message;
};

inline constexpr event_descriptor esys_initialized{"tpm.context.esys_initialized",
                                                   "ESYS context initialized"};
inline constexpr event_descriptor finalized{"tpm.context.finalized", "TPM context finalized"};
inline constexpr event_descriptor pcr_allocate_completed{"tpm.pcr.allocate_completed",
                                                         "PCR allocation completed"};
inline constexpr event_descriptor pcr_auth_policy_set{"tpm.pcr.auth_policy_set",
                                                      "PCR authorization policy set"};
inline constexpr event_descriptor pcr_auth_value_set{"tpm.pcr.auth_value_set",
                                                     "PCR authorization value set"};
inline constexpr event_descriptor pcr_event_completed{"tpm.pcr.event_completed",
                                                      "PCR event completed"};
inline constexpr event_descriptor pcr_extend_completed{"tpm.pcr.extend_completed",
                                                       "PCR extend completed"};
inline constexpr event_descriptor pcr_read_completed{"tpm.pcr.read_completed",
                                                     "PCR read completed"};
inline constexpr event_descriptor pcr_reset_completed{"tpm.pcr.reset_completed",
                                                      "PCR reset completed"};
inline constexpr event_descriptor pcr_tss_error{"tpm.pcr.tss_error", "TPM backend call failed"};
inline constexpr event_descriptor startup_completed{"tpm.context.startup_completed",
                                                    "TPM startup completed"};
inline constexpr event_descriptor startup_invoked{"tpm.context.startup_invoked",
                                                  "TPM startup invoked"};
inline constexpr event_descriptor tcti_configured{"tpm.context.tcti_configured", "TCTI configured"};
inline constexpr event_descriptor tcti_configuring{"tpm.context.tcti_configuring",
                                                   "TCTI configuring"};
inline constexpr event_descriptor tss_error{"tpm.context.tss_error", "TPM backend call failed"};

inline constexpr std::string_view component_tpm2_esys = "tpm2_esys";

namespace fields {

inline constexpr std::string_view abi_version = "abi_version";
inline constexpr std::string_view allocation_success = "allocation_success";
inline constexpr std::string_view bank = "bank";
inline constexpr std::string_view bank_count = "bank_count";
inline constexpr std::string_view component = "component";
inline constexpr std::string_view backend_error_description = "backend_error_description";
inline constexpr std::string_view error_category = "error_category";
inline constexpr std::string_view error_code = "error_code";
inline constexpr std::string_view event = "event";
inline constexpr std::string_view event_size = "event_size";
inline constexpr std::string_view operation = "operation";
inline constexpr std::string_view outcome = "outcome";
inline constexpr std::string_view policy_algorithm = "policy_algorithm";
inline constexpr std::string_view pcr_count = "pcr_count";
inline constexpr std::string_view pcr_index = "pcr_index";
inline constexpr std::string_view result = "result";
inline constexpr std::string_view source = "source";
inline constexpr std::string_view startup_mode = "startup_mode";
inline constexpr std::string_view tcti_kind = "tcti_kind";
inline constexpr std::string_view tcti_name = "tcti_name";
inline constexpr std::string_view tss_layer = "tss_layer";

} // namespace fields

namespace values {

inline constexpr std::string_view backend_error = "backend_error";
inline constexpr std::string_view failure = "failure";
inline constexpr std::string_view input_error = "input_error";
inline constexpr std::string_view resource_error = "resource_error";
inline constexpr std::string_view security_failure = "security_failure";
inline constexpr std::string_view success = "success";

} // namespace values

} // namespace tpmkit::detail::esys::events
