#include <tpmkit/result.h>

int tpmkit_result_header_smoke()
{
    const tpmkit::outcome<int> result{7};
    const tpmkit::error error{tpmkit::error_category::resource_error, "resource"};

    return result.value() + static_cast<int>(error.message.size());
}
