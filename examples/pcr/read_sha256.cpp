#include "example_common.h"

#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/selection.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

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

    const tpmkit::pcr::selection selection{
        tpmkit::hash_algorithm::sha256,
        {tpmkit::pcr::index::firmware_0, tpmkit::pcr::index::firmware_7, tpmkit::pcr::index::debug},
    };
    const auto result = pcr_provider.read(selection);
    if (!result.has_value()) {
        tpmkit::examples::print_error(result.error());
        return EXIT_FAILURE;
    }

    std::cout << "sha256 PCR update counter: " << result->update_counter << "\n";
    for (const auto& value : result->values) {
        std::cout << "pcr" << static_cast<unsigned int>(value.index.value()) << " "
                  << tpmkit::encoding::encode_hex(value.digest.digest()) << "\n";
    }

    return EXIT_SUCCESS;
}
