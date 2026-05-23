#include "impl.h"

#include "error_translation.h"
#include "log_events.h"

#include <tpmkit/logging/noop_logger.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace tpmkit {
namespace {

namespace events = detail::esys::events;

struct resolved_tcti {
    detail::esys::unique_tcti_ptr handle;
};

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

[[nodiscard]] bool is_known_tcti_name(const std::string_view name) noexcept
{
    return name == "device" || name == "mssim" || name == "swtpm" || name == "tabrmd";
}

[[nodiscard]] std::string sanitized_tcti_name(const tcti_string_config& config)
{
    const std::string_view trimmed = trim(config.config);
    const std::size_t colon = trimmed.find(':');

    if (trimmed.empty() || trimmed.size() != config.config.size() ||
        colon == std::string_view::npos || colon == 0U) {
        return {};
    }

    const std::string_view name = trimmed.substr(0U, colon);
    if (is_known_tcti_name(name)) {
        return std::string{name};
    }

    return "<custom>";
}

[[nodiscard]] std::shared_ptr<logger> effective_logger(std::shared_ptr<logger> log)
{
    if (log != nullptr) {
        return log;
    }

    return std::make_shared<noop_logger>();
}

void log_event(logger* const log, const log_level level, const std::string_view event)
{
    if (log == nullptr) {
        return;
    }

    log->log(level, event, gsl::span<const log_field>{});
}

template <std::size_t size>
void log_event(logger* const log, const log_level level, const std::string_view event,
               const std::array<log_field, size>& fields)
{
    if (log == nullptr) {
        return;
    }

    log->log(level, event, gsl::span<const log_field>(fields));
}

void log_tcti_event(logger* const log, const std::string_view event, const std::string_view kind,
                    const std::string_view name)
{
    const std::array<log_field, 2U> fields{{
        {events::fields::tcti_kind, kind},
        {events::fields::tcti_name, name},
    }};

    log_event(log, log_level::info, event, fields);
}

[[nodiscard]] std::string startup_mode_name(const tpm_context_config::startup_mode mode) noexcept
{
    switch (mode) {
    case tpm_context_config::startup_mode::clear:
        return "clear";
    case tpm_context_config::startup_mode::state:
        return "state";
    case tpm_context_config::startup_mode::skip:
        return "skip";
    }

    return "unknown";
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

[[nodiscard]] outcome<void> validate_config(const tpm_context_config& config)
{
    if (!is_valid_startup_mode(config.startup)) {
        return tl::unexpected(error{error_category::input_error, "TPM startup mode is invalid"});
    }

    return {};
}

[[nodiscard]] TPM2_SU startup_type(const tpm_context_config::startup_mode mode) noexcept
{
    switch (mode) {
    case tpm_context_config::startup_mode::clear:
        return TPM2_SU_CLEAR;
    case tpm_context_config::startup_mode::state:
        return TPM2_SU_STATE;
    case tpm_context_config::startup_mode::skip:
        // Unreachable: start_tpm() returns early for skip before calling this function.
        return TPM2_SU_CLEAR;
    }

    return TPM2_SU_CLEAR;
}

[[nodiscard]] std::string abi_version_string(const TSS2_ABI_VERSION& version)
{
    return std::to_string(version.tssCreator) + "." + std::to_string(version.tssFamily) + "." +
           std::to_string(version.tssLevel) + "." + std::to_string(version.tssVersion);
}

void finalize_esys_context(ESYS_CONTEXT* context) noexcept
{
    Esys_Finalize(&context);
}

[[nodiscard]] outcome<resolved_tcti> resolve_tcti(tpm_context_config& config, logger* const log)
{
    if (std::holds_alternative<tcti_string_config>(config.tcti)) {
        const auto& string_config = std::get<tcti_string_config>(config.tcti);
        const std::string tcti_name = sanitized_tcti_name(string_config);
        if (!tcti_name.empty()) {
            log_tcti_event(log, events::tcti_configuring, "string", tcti_name);
        }

        auto loaded = detail::esys::load_tcti(string_config, log);
        if (!loaded.has_value()) {
            return tl::unexpected(loaded.error());
        }

        log_tcti_event(log, events::tcti_configured, "string", tcti_name);
        return resolved_tcti{std::move(loaded.value())};
    }

    auto& owned_config = std::get<tcti_owned_handle>(config.tcti);
    if (owned_config.handle == nullptr) {
        return tl::unexpected(
            error{error_category::input_error, "TCTI owned handle must not be null"});
    }

    if (owned_config.handle.get_deleter() == nullptr) {
        static_cast<void>(owned_config.handle.release());
        return tl::unexpected(
            error{error_category::input_error, "TCTI owned handle deleter must not be null"});
    }

    log_tcti_event(log, events::tcti_configuring, "owned", "<owned>");
    detail::esys::unique_tcti_ptr tcti{std::move(owned_config.handle)};
    log_tcti_event(log, events::tcti_configured, "owned", "<owned>");

    return resolved_tcti{std::move(tcti)};
}

[[nodiscard]] outcome<detail::esys::unique_esys_ptr> initialize_esys(TSS2_TCTI_CONTEXT* const tcti,
                                                                     logger* const log)
{
    ESYS_CONTEXT* raw_esys = nullptr;
    TSS2_ABI_VERSION abi_version = TSS2_ABI_VERSION_CURRENT;
    const TSS2_RC rc = Esys_Initialize(&raw_esys, tcti, &abi_version);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = detail::esys::translate_tss_rc(rc, "esys_initialize", log);
        return tl::unexpected(translated.error());
    }

    detail::esys::unique_esys_ptr esys{raw_esys, &finalize_esys_context};
    const std::string abi_version_value = abi_version_string(abi_version);
    const std::array<log_field, 1U> fields{{
        {events::fields::abi_version, abi_version_value},
    }};
    log_event(log, log_level::info, events::esys_initialized, fields);

    return esys;
}

[[nodiscard]] outcome<void>
start_tpm(ESYS_CONTEXT* const esys, const tpm_context_config::startup_mode mode, logger* const log)
{
    const std::string mode_name = startup_mode_name(mode);
    const std::array<log_field, 1U> invoked_fields{{
        {events::fields::startup_mode, mode_name},
    }};
    log_event(log, log_level::info, events::startup_invoked, invoked_fields);

    if (mode == tpm_context_config::startup_mode::skip) {
        const std::array<log_field, 1U> completed_fields{{
            {events::fields::result, "skipped"},
        }};
        log_event(log, log_level::info, events::startup_completed, completed_fields);
        return {};
    }

    const TSS2_RC rc = Esys_Startup(esys, startup_type(mode));
    if (rc != TSS2_RC_SUCCESS && !detail::esys::is_startup_already_initialized(rc)) {
        auto translated = detail::esys::translate_tss_rc(rc, "esys_startup", log);
        return tl::unexpected(translated.error());
    }

    const std::string_view result = detail::esys::startup_result_field(rc);
    const std::array<log_field, 1U> completed_fields{{
        {events::fields::result, result},
    }};
    log_event(log, log_level::info, events::startup_completed, completed_fields);

    return {};
}

} // namespace

namespace detail::esys {

bool is_startup_already_initialized(const TSS2_RC rc) noexcept
{
    return rc == static_cast<TSS2_RC>(TPM2_RC_INITIALIZE);
}

std::string_view startup_result_field(const TSS2_RC rc) noexcept
{
    return is_startup_already_initialized(rc) ? std::string_view{"already_initialized"}
                                              : std::string_view{"ok"};
}

} // namespace detail::esys

tpm_context::impl::impl(detail::esys::unique_tcti_ptr tcti, detail::esys::unique_esys_ptr esys,
                        std::shared_ptr<logger> log) noexcept
    : tcti_{std::move(tcti)}, esys_{std::move(esys)}, log_{std::move(log)}
{}

tpm_context::impl::~impl() noexcept
{
    log_event(log_.get(), log_level::info, events::finalized);
}

ESYS_CONTEXT* tpm_context::impl::esys() const noexcept
{
    return esys_.get();
}

tpm_context::~tpm_context() noexcept = default;

tpm_context::tpm_context(tpm_context&&) noexcept = default;

tpm_context& tpm_context::operator=(tpm_context&&) noexcept = default;

tpm_context::tpm_context(std::unique_ptr<impl> implementation) noexcept
    : impl_{std::move(implementation)}
{}

outcome<tpm_context> tpm_context::create(tpm_context_config config)
{
    const auto validated = validate_config(config);
    if (!validated.has_value()) {
        return tl::unexpected(validated.error());
    }

    auto log = effective_logger(std::move(config.log));

    auto tcti = resolve_tcti(config, log.get());
    if (!tcti.has_value()) {
        return tl::unexpected(tcti.error());
    }

    auto esys = initialize_esys(tcti.value().handle.get(), log.get());
    if (!esys.has_value()) {
        return tl::unexpected(esys.error());
    }

    auto startup = start_tpm(esys.value().get(), config.startup, log.get());
    if (!startup.has_value()) {
        return tl::unexpected(startup.error());
    }

    auto implementation = std::make_unique<impl>(std::move(tcti.value().handle),
                                                 std::move(esys.value()), std::move(log));
    return tpm_context{std::move(implementation)};
}

void* tpm_context::esys_handle() const noexcept
{
    return impl_ == nullptr ? nullptr : static_cast<void*>(impl_->esys());
}

} // namespace tpmkit
