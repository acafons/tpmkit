#include <tpmkit/testing/fake_tpm_context.h>
#include <tpmkit/testing/in_memory_pcr_observer.h>

#include <utility>

int main()
{
    tpmkit::tpm_context_config config;
    const auto result = tpmkit::testing::fake_tpm_context::create(std::move(config));
    tpmkit::testing::in_memory_pcr_observer observer;
    const auto entries = observer.entries_by_index(tpmkit::pcr::index::debug);

    return (!result.has_value() && result.error().category == tpmkit::error_category::input_error &&
            entries.empty())
               ? 0
               : 1;
}
