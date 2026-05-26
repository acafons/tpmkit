#include "example_common.h"

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr_index.h>
#include <tpmkit/pcr_selection.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

[[nodiscard]] std::array<tpmkit::hash_algorithm, 4U> all_hash_algorithms() noexcept
{
    return {tpmkit::hash_algorithm::sha1, tpmkit::hash_algorithm::sha256,
            tpmkit::hash_algorithm::sha384, tpmkit::hash_algorithm::sha512};
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

    std::size_t active_count = 0U;
    for (const tpmkit::hash_algorithm algorithm : all_hash_algorithms()) {
        const tpmkit::pcr_selection selection{algorithm, {tpmkit::pcr_index::debug}};
        const auto read = provider.value()->read(selection);
        if (!read.has_value() || read.value().values.empty()) {
            std::cout << tpmkit::examples::algorithm_name(algorithm) << " inactive\n";
            continue;
        }

        ++active_count;
        const auto& digest = read.value().values.front().digest;
        std::cout << tpmkit::examples::algorithm_name(algorithm) << " active "
                  << digest.digest().size() << " bytes "
                  << tpmkit::examples::hex_encode(digest.digest()) << "\n";
    }

    return active_count > 0U ? EXIT_SUCCESS : EXIT_FAILURE;
}
