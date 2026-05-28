#include <tpmkit/testing/fake_tpm_context.h>

#include <utility>

int tpmkit_fake_tpm_context_header_self_contained()
{
    tpmkit::outcome<tpmkit::testing::fake_tpm_context> created =
        tpmkit::testing::fake_tpm_context::create("mssim:host=localhost,port=2321",
                                                  tpmkit::tpm_context_config::startup_mode::skip);
    if (!created.has_value()) {
        return 1;
    }

    const tpmkit::testing::fake_tpm_context context = *std::move(created);
    return context.last_config().startup == tpmkit::tpm_context_config::startup_mode::skip ? 0 : 1;
}
