#include <tpmkit/result.h>

namespace tpmkit {

std::string_view error_category_name(const error_category category) noexcept
{
    switch (category) {
    case error_category::input_error:
        return "input_error";
    case error_category::security_failure:
        return "security_failure";
    case error_category::resource_error:
        return "resource_error";
    case error_category::backend_error:
        return "backend_error";
    }

    return "unknown";
}

} // namespace tpmkit
