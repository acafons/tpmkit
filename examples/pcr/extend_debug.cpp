#include "example_common.h"

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>

#include <gsl/span>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view default_sha256_digest =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

} // namespace

int main(const int argc, char** argv)
{
    if (argc > 3) {
        std::cerr << "usage: " << argv[0] << " [tcti-config] [sha256-digest-hex]\n";
        return EXIT_FAILURE;
    }

    std::string tcti_config{tpmkit::examples::default_tcti_config};
    if (argc >= 2) {
        tcti_config = argv[1];
    }

    std::string digest_hex{default_sha256_digest};
    if (argc == 3) {
        digest_hex = argv[2];
    }

    auto digest_bytes = tpmkit::examples::hex_decode(digest_hex);
    if (!digest_bytes.has_value()) {
        tpmkit::examples::print_error(digest_bytes.error());
        return EXIT_FAILURE;
    }

    if (digest_bytes.value().size() != tpmkit::digest_size(tpmkit::hash_algorithm::sha256)) {
        tpmkit::examples::print_error(tpmkit::error{
            tpmkit::error_category::input_error, "sha256 digest input must be exactly 32 bytes"});
        return EXIT_FAILURE;
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

    const auto reset = provider.value()->reset(tpmkit::pcr::index::debug);
    if (!reset.has_value()) {
        tpmkit::examples::print_error(reset.error());
        return EXIT_FAILURE;
    }

    std::vector<tpmkit::pcr::digest_value> digests;
    digests.emplace_back(tpmkit::hash_algorithm::sha256, std::move(digest_bytes.value()));
    const auto extend =
        provider.value()->extend(tpmkit::pcr::index::debug, gsl::make_span(digests));
    if (!extend.has_value()) {
        tpmkit::examples::print_error(extend.error());
        return EXIT_FAILURE;
    }

    const auto current = tpmkit::examples::read_pcr_digest(
        *provider.value(), tpmkit::hash_algorithm::sha256, tpmkit::pcr::index::debug);
    if (!current.has_value()) {
        tpmkit::examples::print_error(current.error());
        return EXIT_FAILURE;
    }

    std::cout << "pcr16 extended with sha256 " << digest_hex << "\n";
    std::cout << "pcr16 current " << tpmkit::examples::hex_encode(current.value()) << "\n";
    return EXIT_SUCCESS;
}
