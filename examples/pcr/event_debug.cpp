#include "example_common.h"

#include <tpmkit/pcr_index.h>
#include <tpmkit/pcr_observer.h>
#include <tpmkit/pcr_selection.h>

#include <gsl/span>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view default_event_text = "tpmkit PCR event example";

class counting_pcr_observer final : public tpmkit::pcr_observer {
public:
    [[nodiscard]] std::size_t event_count() const noexcept
    {
        return event_count_;
    }

    [[nodiscard]] std::size_t extend_count() const noexcept
    {
        return extend_count_;
    }

    void on_event(tpmkit::pcr_index, gsl::span<const std::uint8_t>,
                  const tpmkit::pcr_event_result&) noexcept final
    {
        ++event_count_;
    }

    void on_extend(tpmkit::pcr_index, gsl::span<const tpmkit::pcr_digest_value>) noexcept final
    {
        ++extend_count_;
    }

private:
    std::size_t event_count_ = 0U;
    std::size_t extend_count_ = 0U;
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

    auto context = tpmkit::examples::create_context(std::move(tcti_config));
    if (!context.has_value()) {
        tpmkit::examples::print_error(context.error());
        return EXIT_FAILURE;
    }

    counting_pcr_observer observer;
    auto provider = context.value().create_pcr_provider(&observer);
    if (!provider.has_value()) {
        tpmkit::examples::print_error(provider.error());
        return EXIT_FAILURE;
    }

    const auto reset = provider.value()->reset(tpmkit::pcr_index::debug);
    if (!reset.has_value()) {
        tpmkit::examples::print_error(reset.error());
        return EXIT_FAILURE;
    }

    const std::vector<std::uint8_t> event_data = tpmkit::examples::bytes_from_text(event_text);
    const auto event =
        provider.value()->event(tpmkit::pcr_index::debug, gsl::make_span(event_data));
    if (!event.has_value()) {
        tpmkit::examples::print_error(event.error());
        return EXIT_FAILURE;
    }

    std::cout << "pcr16 event digests:\n";
    for (const auto& digest : event.value().digests) {
        if (digest.algorithm() == tpmkit::hash_algorithm::sha256) {
            std::cout << "sha256 " << tpmkit::examples::hex_encode(digest.digest()) << "\n";
        }
    }

    const tpmkit::pcr_selection selection{tpmkit::hash_algorithm::sha256,
                                          {tpmkit::pcr_index::debug}};
    const auto read = provider.value()->read(selection);
    if (!read.has_value()) {
        tpmkit::examples::print_error(read.error());
        return EXIT_FAILURE;
    }

    for (const auto& value : read.value().values) {
        std::cout << "pcr16 current " << tpmkit::examples::hex_encode(value.digest.digest())
                  << "\n";
    }
    std::cout << "observer events: " << observer.event_count()
              << ", observer extends: " << observer.extend_count() << "\n";

    return EXIT_SUCCESS;
}
