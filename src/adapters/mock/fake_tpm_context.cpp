#include <tpmkit/testing/fake_tpm_context.h>

#include <tl/expected.hpp>

#include <cctype>
#include <string_view>
#include <utility>
#include <variant>

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

[[nodiscard]] outcome<tpm_context_config> validate(tpm_context_config config)
{
    if (const auto* const string_tcti = std::get_if<tcti_string_config>(&config.tcti)) {
        if (!is_valid_string_config(*string_tcti)) {
            return tl::unexpected(
                error{error_category::input_error, "TCTI configuration must use name:args format"});
        }
    }

    if (auto* const owned_tcti = std::get_if<tcti_owned_handle>(&config.tcti)) {
        if (owned_tcti->handle == nullptr) {
            return tl::unexpected(
                error{error_category::input_error, "Owned TCTI handle must not be null"});
        }

        if (owned_tcti->handle.get_deleter() == nullptr) {
            static_cast<void>(owned_tcti->handle.release());
            return tl::unexpected(
                error{error_category::input_error, "Owned TCTI handle deleter must not be null"});
        }
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

    return fake_tpm_context{std::move(validated).value()};
}

const tpm_context_config& fake_tpm_context::last_config() const noexcept
{
    return config_;
}

void* fake_tpm_context::esys_handle() const noexcept
{
    return nullptr;
}

} // namespace tpmkit::testing
