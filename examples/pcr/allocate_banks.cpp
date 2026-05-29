#include "example_common.h"

#include <tpmkit/pcr/bank.h>

#include <gsl/span>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view add_sha384_flag = "--add-sha384";

[[nodiscard]] bool contains_algorithm(const std::vector<tpmkit::hash_algorithm>& algorithms,
                                      const tpmkit::hash_algorithm algorithm)
{
    return std::find(algorithms.begin(), algorithms.end(), algorithm) != algorithms.end();
}

[[nodiscard]] bool parse_args(const int argc, char** argv, std::string& tcti_config,
                              bool& add_sha384)
{
    if (argc > 3) {
        return false;
    }

    bool has_tcti_config = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == add_sha384_flag) {
            add_sha384 = true;
            continue;
        }

        if (has_tcti_config) {
            return false;
        }
        has_tcti_config = true;
        tcti_config = std::string{argument};
    }

    return true;
}

} // namespace

int main(const int argc, char** argv)
{
    std::string tcti_config{tpmkit::examples::default_tcti_config};
    bool add_sha384 = false;
    if (!parse_args(argc, argv, tcti_config, add_sha384)) {
        std::cerr << "usage: " << argv[0] << " [tcti-config] [--add-sha384]\n";
        return EXIT_FAILURE;
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

    auto requested = tpmkit::examples::active_pcr_algorithms(pcr_provider);
    if (requested.empty()) {
        std::cerr << "no active PCR banks found\n";
        return EXIT_FAILURE;
    }

    if (add_sha384 && !contains_algorithm(requested, tpmkit::hash_algorithm::sha384)) {
        requested.push_back(tpmkit::hash_algorithm::sha384);
    }

    std::vector<tpmkit::pcr::bank> banks;
    banks.reserve(requested.size());
    for (const tpmkit::hash_algorithm algorithm : requested) {
        banks.emplace_back(algorithm);
    }

    const auto allocation = pcr_provider.allocate(gsl::make_span(banks));
    if (!allocation.has_value()) {
        tpmkit::examples::print_error(allocation.error());
        return EXIT_FAILURE;
    }

    std::cout << "requested PCR banks:";
    for (const tpmkit::hash_algorithm algorithm : requested) {
        std::cout << " " << tpmkit::hash_algorithm_name(algorithm);
    }
    std::cout << "\n";
    std::cout << "allocation success: " << (allocation->allocation_success ? "true" : "false")
              << "\n";
    std::cout << "max PCR: " << allocation->max_pcr << "\n";
    std::cout << "size needed: " << allocation->size_needed << "\n";
    std::cout << "size available: " << allocation->size_available << "\n";

    return allocation->allocation_success ? EXIT_SUCCESS : EXIT_FAILURE;
}
