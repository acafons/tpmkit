#include <tpmkit/testing/fake_tpm_context.h>

#include <tl/expected.hpp>

#include <cctype>
#include <string_view>
#include <utility>

namespace tpmkit::testing {

namespace {

[[nodiscard]] bool is_space(const char value) noexcept
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

[[nodiscard]] std::string_view trim(const std::string_view value) noexcept
{
    std::size_t first = 0U;
    std::size_t last = value.size();

    while (first < last && is_space(value[first])) {
        ++first;
    }

    while (last > first && is_space(value[last - 1U])) {
        --last;
    }

    return value.substr(first, last - first);
}

[[nodiscard]] bool has_name_args_shape(const std::string_view value) noexcept
{
    const std::size_t colon = value.find(':');

    return colon != std::string_view::npos && colon > 0U;
}

[[nodiscard]] bool is_valid_string_config(const tcti_string_config& config) noexcept
{
    const std::string_view trimmed = trim(config.config);

    return !trimmed.empty() && trimmed.size() == config.config.size() &&
           has_name_args_shape(trimmed);
}

[[nodiscard]] bool is_valid_startup_mode(const tpm_context_config::startup_mode mode) noexcept
{
    switch (mode) {
    case tpm_context_config::startup_mode::clear:
    case tpm_context_config::startup_mode::state:
    case tpm_context_config::startup_mode::skip:
        return true;
    }

    return false;
}

[[nodiscard]] outcome<tpm_context_config> validate(tpm_context_config config)
{
    if (!is_valid_startup_mode(config.startup)) {
        return tl::unexpected(error{error_category::input_error, "TPM startup mode is invalid"});
    }

    if (!is_valid_string_config(config.tcti)) {
        return tl::unexpected(
            error{error_category::input_error, "TCTI configuration must use name:args format"});
    }

    return config;
}

} // namespace

fake_tpm_context::fake_tpm_context(tpm_context_config config) noexcept : config_{std::move(config)}
{}

fake_tpm_context::~fake_tpm_context() noexcept = default;

fake_tpm_context::fake_tpm_context(fake_tpm_context&& other) noexcept = default;

fake_tpm_context& fake_tpm_context::operator=(fake_tpm_context&& other) noexcept = default;

outcome<fake_tpm_context> fake_tpm_context::create(tpm_context_config config)
{
    outcome<tpm_context_config> validated = validate(std::move(config));
    if (!validated.has_value()) {
        return tl::unexpected(std::move(validated.error()));
    }

    return fake_tpm_context{*std::move(validated)};
}

outcome<fake_tpm_context> fake_tpm_context::create(std::string tcti_config,
                                                   const tpm_context_config::startup_mode startup,
                                                   std::shared_ptr<logger> log)
{
    tpm_context_config config;
    config.tcti = tcti_string_config{std::move(tcti_config)};
    config.startup = startup;
    config.log = std::move(log);
    return create(std::move(config));
}

const tpm_context_config& fake_tpm_context::last_config() const noexcept
{
    return config_;
}

outcome<std::unique_ptr<pcr::provider>>
fake_tpm_context::create_pcr_provider(pcr::observer* const observer)
{
    static_cast<void>(observer);
    return tl::unexpected(
        error{error_category::resource_error, "Fake TPM context has no TPM backend"});
}

} // namespace tpmkit::testing
