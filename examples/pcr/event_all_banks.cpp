#include "example_common.h"

#include <tpmkit/pcr/index.h>

#include <gsl/span>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view default_event_text = "tpmkit PCR all-bank event example";

} // namespace

int main(const int argc, char** argv)
{
    if (argc > 3) {
        std::cerr << "usage: " << argv[0] << " [tcti-config] [event-text]\n";
        return EXIT_FAILURE;
    }

    std::string tcti_config{tpmkit::examples::default_tcti_config};
    if (argc >= 2) {
        tcti_config = argv[1];
    }

    std::string event_text{default_event_text};
    if (argc == 3) {
        event_text = argv[2];
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

    const auto reset = pcr_provider.reset(tpmkit::pcr::index::debug);
    if (!reset.has_value()) {
        tpmkit::examples::print_error(reset.error());
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> event_data = tpmkit::examples::make_event_bytes(event_text);
    const auto event = pcr_provider.event(tpmkit::pcr::index::debug, gsl::make_span(event_data));
    if (!event.has_value()) {
        tpmkit::examples::print_error(event.error());
        return EXIT_FAILURE;
    }

    std::cout << "pcr16 event digests:\n";
    for (const auto& digest : event->digests) {
        std::cout << tpmkit::hash_algorithm_name(digest.algorithm()) << " "
                  << tpmkit::encoding::encode_hex(digest.digest()) << "\n";
    }

    const auto active = tpmkit::examples::active_pcr_algorithms(pcr_provider);
    std::cout << "pcr16 current active values:\n";
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

    return event->digests.empty() ? EXIT_FAILURE : EXIT_SUCCESS;
}
