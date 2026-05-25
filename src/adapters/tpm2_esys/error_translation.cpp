#include "error_translation.h"

#include "log_events.h"

#include <tpmkit/logging/logger.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace tpmkit::detail::esys {
namespace {

struct mapped_error {
    error_category category;
    std::string_view message;
};

struct mapping_entry {
    TSS2_RC layer;
    TSS2_RC base;
    mapped_error error;
};

constexpr std::string_view input_message = "input rejected by TPM";
constexpr std::string_view security_message = "TPM verification failed";
constexpr std::string_view resource_message = "TPM operation cannot be completed";
constexpr std::string_view backend_message = "TPM backend reported an error";

constexpr mapped_error input_error{error_category::input_error, input_message};
constexpr mapped_error security_error{error_category::security_failure, security_message};
constexpr mapped_error resource_error{error_category::resource_error, resource_message};
constexpr mapped_error backend_error{error_category::backend_error, backend_message};

[[nodiscard]] constexpr mapping_entry map_entry(const TSS2_RC layer, const TSS2_RC base,
                                                const mapped_error error) noexcept
{
    return mapping_entry{layer, base, error};
}

constexpr std::array<mapping_entry, 50> documented_mappings{{
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_AUTH_FAIL, security_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_BAD_AUTH, security_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_HASH, input_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_HANDLE, input_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_LOCALITY, resource_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_LOCKOUT, security_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_MEMORY, resource_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_POLICY_FAIL, security_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_RANGE, input_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_SIZE, input_error),
    map_entry(TSS2_TPM_RC_LAYER, TPM2_RC_VALUE, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_GENERAL_FAILURE, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_NOT_IMPLEMENTED, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_ABI_MISMATCH, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_REFERENCE, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_INSUFFICIENT_BUFFER, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_SEQUENCE, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_INVALID_SESSIONS, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_TRY_AGAIN, resource_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_IO_ERROR, resource_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_VALUE, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_NO_DECRYPT_PARAM, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_NO_ENCRYPT_PARAM, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_SIZE, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_MALFORMED_RESPONSE, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_INSUFFICIENT_CONTEXT, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_INSUFFICIENT_RESPONSE, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_INCOMPATIBLE_TCTI, resource_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_TCTI_STRUCTURE, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_MEMORY, resource_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_BAD_TR, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_MULTIPLE_DECRYPT_SESSIONS, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_MULTIPLE_ENCRYPT_SESSIONS, input_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_NOT_SUPPORTED, backend_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_RSP_AUTH_FAILED, security_error),
    map_entry(TSS2_ESAPI_RC_LAYER, TSS2_BASE_RC_CALLBACK_NULL, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_GENERAL_FAILURE, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_NOT_IMPLEMENTED, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_BAD_CONTEXT, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_ABI_MISMATCH, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_BAD_REFERENCE, input_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_INSUFFICIENT_BUFFER, input_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_BAD_SEQUENCE, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_NO_CONNECTION, resource_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_TRY_AGAIN, resource_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_IO_ERROR, resource_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_BAD_VALUE, input_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_NOT_PERMITTED, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_MALFORMED_RESPONSE, backend_error),
    map_entry(TSS2_TCTI_RC_LAYER, TSS2_BASE_RC_NOT_SUPPORTED, backend_error),
}};

[[nodiscard]] TSS2_RC rc_layer(const TSS2_RC rc) noexcept
{
    return rc & TSS2_RC_LAYER_MASK;
}

[[nodiscard]] TSS2_RC rc_base(const TSS2_RC rc) noexcept
{
    return rc & 0xffffU;
}

[[nodiscard]] std::string_view layer_name(const TSS2_RC layer) noexcept
{
    switch (layer) {
    case TSS2_TPM_RC_LAYER:
        return "tpm";
    case TSS2_TCTI_RC_LAYER:
        return "tcti";
    case TSS2_ESAPI_RC_LAYER:
        return "esapi";
    case TSS2_SYS_RC_LAYER:
        return "sys";
    case TSS2_MU_RC_LAYER:
        return "mu";
    default:
        return "unknown";
    }
}

[[nodiscard]] mapped_error translate_layer_base(const TSS2_RC layer, const TSS2_RC base) noexcept
{
    for (const auto& entry : documented_mappings) {
        if (entry.layer == layer && entry.base == base) {
            return entry.error;
        }
    }

    return backend_error;
}

[[nodiscard]] std::string format_hex(const TSS2_RC rc)
{
    std::array<char, 11> buffer{};
    static_cast<void>(std::snprintf(buffer.data(), buffer.size(), "0x%08x", rc));

    return std::string{buffer.data()};
}

void log_tss_error(logger* const log, const TSS2_RC rc, const TSS2_RC layer,
                   const std::string_view operation, const std::string_view error_event)
{
    if (log == nullptr) {
        return;
    }

    const std::string rc_hex = format_hex(rc);
    const std::array<log_field, 3> fields{{
        {events::fields::operation, operation},
        {events::fields::tss_rc_hex, rc_hex},
        {events::fields::tss_layer, layer_name(layer)},
    }};

    log->log(log_level::error, error_event, gsl::span<const log_field>(fields));
}

} // namespace

outcome<void> translate_tss_rc(const TSS2_RC rc, const std::string_view operation,
                               logger* const log)
{
    return translate_tss_rc(rc, operation, log, events::tss_error);
}

outcome<void> translate_tss_rc(const TSS2_RC rc, const std::string_view operation,
                               logger* const log, const std::string_view error_event)
{
    if (rc == TSS2_RC_SUCCESS) {
        return {};
    }

    const TSS2_RC layer = rc_layer(rc);
    log_tss_error(log, rc, layer, operation, error_event);

    const mapped_error mapped = translate_layer_base(layer, rc_base(rc));
    return tl::unexpected(error{mapped.category, std::string{mapped.message}});
}

} // namespace tpmkit::detail::esys
