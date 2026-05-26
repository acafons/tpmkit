#include "example_common.h"

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr_index.h>

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

    auto context = tpmkit::examples::create_context(std::move(tcti_config));
    if (!context.has_value()) {
        tpmkit::examples::print_error(context.error());
        return EXIT_FAILURE;
    }

    auto provider = context.value().create_pcr_provider();
    if (!provider.has_value()) {
        tpmkit::examples::print_error(provider.error());
        return EXIT_FAILURE;
    }

    const auto before = tpmkit::examples::read_pcr_digest(
        *provider.value(), tpmkit::hash_algorithm::sha256, tpmkit::pcr_index::debug);
    if (!before.has_value()) {
        tpmkit::examples::print_error(before.error());
        return EXIT_FAILURE;
    }

    const auto reset = provider.value()->reset(tpmkit::pcr_index::debug);
    if (!reset.has_value()) {
        tpmkit::examples::print_error(reset.error());
        return EXIT_FAILURE;
    }

    const auto after = tpmkit::examples::read_pcr_digest(
        *provider.value(), tpmkit::hash_algorithm::sha256, tpmkit::pcr_index::debug);
    if (!after.has_value()) {
        tpmkit::examples::print_error(after.error());
        return EXIT_FAILURE;
    }

    std::cout << "pcr16 before " << tpmkit::examples::hex_encode(before.value()) << "\n";
    std::cout << "pcr16 after  " << tpmkit::examples::hex_encode(after.value()) << "\n";
    return EXIT_SUCCESS;
}
