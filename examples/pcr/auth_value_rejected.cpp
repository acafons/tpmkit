#include "example_common.h"

#include <tpmkit/pcr/index.h>
#include <tpmkit/secret_buffer.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main(const int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "usage: " << argv[0] << " [tcti-config]\n";
        return EXIT_FAILURE;
    }

    std::string tcti_config{tpmkit::examples::default_tcti_config};
    if (argc == 2) {
        tcti_config = argv[1];
    }

    auto context = tpmkit::tpm_context::create(std::move(tcti_config));
    if (!context.has_value()) {
        tpmkit::examples::print_error(context.error());
        return EXIT_FAILURE;
    }

    auto provider = context->create_pcr_provider();
    if (!provider.has_value()) {
        tpmkit::examples::print_error(provider.error());
        return EXIT_FAILURE;
    }
    auto& pcr_provider = *provider.value();

    tpmkit::secret_buffer auth{std::vector<std::uint8_t>(16U, 0xa5U)};
    const auto rejected = pcr_provider.set_auth_value(tpmkit::pcr::index::debug, std::move(auth));
    if (rejected.has_value()) {
        std::cerr << "non-empty PCR auth value was accepted unexpectedly\n";
        return EXIT_FAILURE;
    }

    if (rejected.error().category != tpmkit::error_category::resource_error) {
        tpmkit::examples::print_error(rejected.error());
        return EXIT_FAILURE;
    }

    std::cout << "non-empty PCR auth value rejected without secure transport\n";
    return EXIT_SUCCESS;
}
