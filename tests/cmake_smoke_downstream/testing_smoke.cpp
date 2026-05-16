#include <tpmkit/testing/fake_tpm_context.h>

int main()
{
    const auto result = tpmkit::testing::fake_tpm_context::create({});

    return (!result.has_value() &&
            result.error().category == tpmkit::error_category::input_error)
        ? 0
        : 1;
}
