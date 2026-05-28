#include <tpmkit/result.h>

int tpmkit_result_header_smoke()
{
    const tpmkit::outcome<int> result{7};
    const tpmkit::error error{tpmkit::error_category::resource_error, "resource"};
    const std::string_view category = tpmkit::error_category_name(error.category);

    return *result + static_cast<int>(error.message.size()) + static_cast<int>(category.size());
}
