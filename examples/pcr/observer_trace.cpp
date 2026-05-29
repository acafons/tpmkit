#include "example_common.h"

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/digest_value.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/observer.h>

#include <gsl/span>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::string_view default_event_text = "tpmkit observer example";
constexpr std::string_view sha256_digest =
    "101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f";

struct observed_measurement {
    std::size_t digest_count;
    std::size_t event_size;
    std::string operation;
};

class tracing_pcr_observer final : public tpmkit::pcr::observer {
public:
    [[nodiscard]] const std::vector<observed_measurement>& records() const noexcept
    {
        return records_;
    }

    void on_event(tpmkit::pcr::index, gsl::span<const std::uint8_t> event_data,
                  const tpmkit::pcr::event_result& result) noexcept final
    {
        try {
            records_.push_back(
                observed_measurement{result.digests.size(), event_data.size(), "event"});
        } catch (...) {
        }
    }

    void on_extend(tpmkit::pcr::index,
                   const gsl::span<const tpmkit::pcr::digest_value> digests) noexcept final
    {
        try {
            records_.push_back(observed_measurement{digests.size(), 0U, "extend"});
        } catch (...) {
        }
    }

private:
    std::vector<observed_measurement> records_;
};

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

    auto digest_bytes = tpmkit::encoding::decode_hex(sha256_digest);
    if (!digest_bytes.has_value()) {
        tpmkit::examples::print_error(digest_bytes.error());
        return EXIT_FAILURE;
    }

    auto context = tpmkit::tpm_context::create(std::move(tcti_config));
    if (!context.has_value()) {
        tpmkit::examples::print_error(context.error());
        return EXIT_FAILURE;
    }

    tracing_pcr_observer observer;
    auto provider = context->create_pcr_provider(&observer);
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

    std::vector<tpmkit::pcr::digest_value> digests;
    digests.emplace_back(tpmkit::hash_algorithm::sha256, *std::move(digest_bytes));
    const auto extend = pcr_provider.extend(tpmkit::pcr::index::debug, gsl::make_span(digests));
    if (!extend.has_value()) {
        tpmkit::examples::print_error(extend.error());
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> event_data = tpmkit::examples::bytes_from_text(event_text);
    const auto event = pcr_provider.event(tpmkit::pcr::index::debug, gsl::make_span(event_data));
    if (!event.has_value()) {
        tpmkit::examples::print_error(event.error());
        return EXIT_FAILURE;
    }

    for (const auto& record : observer.records()) {
        std::cout << record.operation << " digests=" << record.digest_count
                  << " event_bytes=" << record.event_size << "\n";
    }

    return observer.records().size() == 2U ? EXIT_SUCCESS : EXIT_FAILURE;
}
