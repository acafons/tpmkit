#include "example_common.h"

#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>

#include <gsl/span>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::vector<std::uint8_t> make_digest_bytes(const tpmkit::hash_algorithm algorithm,
                                                          const std::uint8_t fill)
{
    return std::vector<std::uint8_t>(tpmkit::digest_size(algorithm), fill);
}

} // namespace

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

    const auto active = tpmkit::examples::active_pcr_algorithms(pcr_provider);
    if (active.empty()) {
        std::cerr << "no active PCR banks found\n";
        return EXIT_FAILURE;
    }

    const auto reset = pcr_provider.reset(tpmkit::pcr::index::debug);
    if (!reset.has_value()) {
        tpmkit::examples::print_error(reset.error());
        return EXIT_FAILURE;
    }

    std::vector<tpmkit::pcr::digest_value> digests;
    digests.reserve(active.size());
    for (std::size_t offset = 0U; offset < active.size(); ++offset) {
        const tpmkit::hash_algorithm algorithm = active[offset];
        const auto fill = static_cast<std::uint8_t>(0x10U + offset);
        digests.emplace_back(algorithm, make_digest_bytes(algorithm, fill));
    }

    const auto extend = pcr_provider.extend(tpmkit::pcr::index::debug, gsl::make_span(digests));
    if (!extend.has_value()) {
        tpmkit::examples::print_error(extend.error());
        return EXIT_FAILURE;
    }

    std::cout << "pcr16 extended across " << digests.size() << " active banks\n";
    for (const auto& digest : digests) {
        std::cout << tpmkit::hash_algorithm_name(digest.algorithm()) << " input "
                  << tpmkit::encoding::encode_hex(digest.digest()) << "\n";
    }

    std::cout << "pcr16 current values:\n";
    for (const tpmkit::hash_algorithm algorithm : active) {
        const auto current =
            tpmkit::examples::read_pcr_digest(pcr_provider, algorithm, tpmkit::pcr::index::debug);
        if (!current.has_value()) {
            tpmkit::examples::print_error(current.error());
            return EXIT_FAILURE;
        }

        std::cout << tpmkit::hash_algorithm_name(algorithm) << " "
                  << tpmkit::encoding::encode_hex(*current) << "\n";
    }

    return EXIT_SUCCESS;
}
