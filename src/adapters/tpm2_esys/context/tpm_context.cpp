#include "impl.h"

#include "../pcr/esys_pcr_provider.h"
#include "../support/error_translation.h"
#include "../support/log_events.h"

#include <tpmkit/logging/noop_logger.h>
#include <tpmkit/tpm2_esys/owned_tcti_context.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace tpmkit {
namespace {

namespace events = detail::tpm2_esys::events;

struct resolved_tcti {
    detail::tpm2_esys::unique_tcti_ptr handle;
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

    return std::shared_ptr<logger>(&noop_logger::instance(), [](logger*) noexcept {});
}

template <std::size_t size>
void log_event(logger* const log, const log_level level, const events::event_descriptor event,
               const std::array<log_field, size>& fields)
{
    if (log == nullptr) {
        return;
    }

    std::array<log_field, size + 2U> merged{};
    merged[0U] = {events::fields::event, event.name};
    merged[1U] = {events::fields::component, events::component_tpm2_esys};
    for (std::size_t index = 0U; index < size; ++index) {
        merged[index + 2U] = fields[index];
    }

    log->log(level, event.message, gsl::span<const log_field>(merged));
}

void log_tcti_event(logger* const log, const events::event_descriptor event,
                    const std::string_view kind, const std::string_view name)
{
    const std::array<log_field, 3U> fields{{
        {events::fields::outcome, events::values::success},
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

[[nodiscard]] outcome<resolved_tcti>
resolve_tcti(const tcti_string_config& string_config, logger* const log,
             const detail::tpm2_esys::tcti_loader_api& tcti_api)
{
    const std::string tcti_name = sanitized_tcti_name(string_config);
    if (!tcti_name.empty()) {
        log_tcti_event(log, events::tcti_configuring, "string", tcti_name);
    }

    auto loaded = detail::tpm2_esys::load_tcti(string_config, log, tcti_api);
    if (!loaded.has_value()) {
        return tl::unexpected(loaded.error());
    }

    log_tcti_event(log, events::tcti_configured, "string", tcti_name);
    return resolved_tcti{*std::move(loaded)};
}

[[nodiscard]] outcome<detail::tpm2_esys::unique_esys_ptr>
initialize_esys(TSS2_TCTI_CONTEXT* const tcti, logger* const log,
                const detail::tpm2_esys::esys_api& api)
{
    ESYS_CONTEXT* raw_esys = nullptr;
    TSS2_ABI_VERSION abi_version = TSS2_ABI_VERSION_CURRENT;
    const TSS2_RC rc = api.initialize(&raw_esys, tcti, &abi_version);
    if (rc != TSS2_RC_SUCCESS) {
        auto translated = detail::tpm2_esys::translate_tss_rc(rc, "esys_initialize", log,
                                                              events::tss_error, api.decode_error);
        return tl::unexpected(translated.error());
    }

    detail::tpm2_esys::unique_esys_ptr esys{raw_esys,
                                            detail::tpm2_esys::esys_context_deleter{&api}};
    const std::string abi_version_value = abi_version_string(abi_version);
    const std::array<log_field, 2U> fields{{
        {events::fields::abi_version, abi_version_value},
        {events::fields::outcome, events::values::success},
    }};
    log_event(log, log_level::info, events::esys_initialized, fields);

    return esys;
}

[[nodiscard]] outcome<void> start_tpm(ESYS_CONTEXT* const esys,
                                      const tpm_context_config::startup_mode mode,
                                      logger* const log, const detail::tpm2_esys::esys_api& api)
{
    const std::string mode_name = startup_mode_name(mode);
    const std::array<log_field, 2U> invoked_fields{{
        {events::fields::outcome, events::values::success},
        {events::fields::startup_mode, mode_name},
    }};
    log_event(log, log_level::info, events::startup_invoked, invoked_fields);

    if (mode == tpm_context_config::startup_mode::skip) {
        const std::array<log_field, 3U> completed_fields{{
            {events::fields::outcome, events::values::success},
            {events::fields::startup_mode, mode_name},
            {events::fields::result, "skipped"},
        }};
        log_event(log, log_level::info, events::startup_completed, completed_fields);
        return {};
    }

    const TSS2_RC rc = api.startup(esys, startup_type(mode));
    if (rc != TSS2_RC_SUCCESS && !detail::tpm2_esys::is_startup_already_initialized(rc)) {
        auto translated = detail::tpm2_esys::translate_tss_rc(rc, "esys_startup", log,
                                                              events::tss_error, api.decode_error);
        return tl::unexpected(translated.error());
    }

    const std::string_view result = detail::tpm2_esys::startup_result_field(rc);
    const std::array<log_field, 3U> completed_fields{{
        {events::fields::outcome, events::values::success},
        {events::fields::startup_mode, mode_name},
        {events::fields::result, result},
    }};
    log_event(log, log_level::info, events::startup_completed, completed_fields);

    return {};
}

} // namespace

namespace detail::tpm2_esys {

bool is_startup_already_initialized(const TSS2_RC rc) noexcept
{
    return rc == static_cast<TSS2_RC>(TPM2_RC_INITIALIZE);
}

std::string_view startup_result_field(const TSS2_RC rc) noexcept
{
    return is_startup_already_initialized(rc) ? std::string_view{"already_initialized"}
                                              : std::string_view{"ok"};
}

} // namespace detail::tpm2_esys

namespace detail {

struct tpm_context_factory {
    [[nodiscard]] static outcome<tpm_context>
    create_from_resolved_tcti(tpm2_esys::unique_tcti_ptr tcti,
                              const tpm_context_config::startup_mode startup,
                              std::shared_ptr<logger> log, const tpm2_esys::esys_api& api)
    {
        auto esys = initialize_esys(tcti.get(), log.get(), api);
        if (!esys.has_value()) {
            return tl::unexpected(esys.error());
        }

        auto started = start_tpm(esys->get(), startup, log.get(), api);
        if (!started.has_value()) {
            return tl::unexpected(started.error());
        }

        auto implementation = std::make_unique<tpm_context::impl>(std::move(tcti), *std::move(esys),
                                                                  std::move(log), api);
        return tpm_context{std::move(implementation)};
    }
};

} // namespace detail

namespace detail::tpm2_esys {

outcome<tpm_context> create_context_from_config(tpm_context_config config,
                                                const tcti_loader_api& tcti_api,
                                                const esys_api& api)
{
    const auto validated = validate_config(config);
    if (!validated.has_value()) {
        return tl::unexpected(validated.error());
    }

    auto log = effective_logger(std::move(config.log));

    auto tcti = resolve_tcti(config.tcti, log.get(), tcti_api);
    if (!tcti.has_value()) {
        return tl::unexpected(tcti.error());
    }

    return ::tpmkit::detail::tpm_context_factory::create_from_resolved_tcti(
        std::move(tcti->handle), config.startup, std::move(log), api);
}

outcome<tpm_context> create_context_from_owned_tcti(::tpmkit::tpm2_esys::owned_tcti_context tcti,
                                                    const tpm_context_config::startup_mode startup,
                                                    std::shared_ptr<logger> log,
                                                    const esys_api& api)
{
    if (!is_valid_startup_mode(startup)) {
        return tl::unexpected(error{error_category::input_error, "TPM startup mode is invalid"});
    }

    if (tcti.handle == nullptr) {
        return tl::unexpected(
            error{error_category::input_error, "TCTI owned handle must not be null"});
    }

    if (tcti.handle.get_deleter() == nullptr) {
        static_cast<void>(tcti.handle.release());
        return tl::unexpected(
            error{error_category::input_error, "TCTI owned handle deleter must not be null"});
    }

    auto effective_log = effective_logger(std::move(log));
    log_tcti_event(effective_log.get(), events::tcti_configuring, "owned_handle", "<owned>");
    unique_tcti_ptr owned_tcti{std::move(tcti.handle)};
    log_tcti_event(effective_log.get(), events::tcti_configured, "owned_handle", "<owned>");

    return ::tpmkit::detail::tpm_context_factory::create_from_resolved_tcti(
        std::move(owned_tcti), startup, std::move(effective_log), api);
}

} // namespace detail::tpm2_esys

tpm_context::impl::impl(detail::tpm2_esys::unique_tcti_ptr tcti,
                        detail::tpm2_esys::unique_esys_ptr esys, std::shared_ptr<logger> log,
                        const detail::tpm2_esys::esys_api& api) noexcept
    : tcti_{std::move(tcti)}, esys_{std::move(esys)}, log_{std::move(log)}, api_{api}
{}

tpm_context::impl::~impl() noexcept
{
    const std::array<log_field, 1U> fields{{
        {events::fields::outcome, events::values::success},
    }};
    log_event(log_.get(), log_level::info, events::finalized, fields);
}

ESYS_CONTEXT* tpm_context::impl::esys() const noexcept
{
    return esys_.get();
}

const detail::tpm2_esys::esys_api& tpm_context::impl::esys_api() const noexcept
{
    return api_;
}

logger& tpm_context::impl::log() const noexcept
{
    return *log_;
}

tpm_context::~tpm_context() noexcept = default;

tpm_context::tpm_context(tpm_context&&) noexcept = default;

tpm_context& tpm_context::operator=(tpm_context&&) noexcept = default;

tpm_context::tpm_context(std::unique_ptr<impl> implementation) noexcept
    : impl_{std::move(implementation)}
{}

outcome<tpm_context> tpm_context::create(tpm_context_config config)
{
    return detail::tpm2_esys::create_context_from_config(
        std::move(config), detail::tpm2_esys::default_tcti_loader_api(),
        detail::tpm2_esys::default_esys_api());
}

outcome<tpm_context> tpm_context::create(std::string tcti_config,
                                         const tpm_context_config::startup_mode startup,
                                         std::shared_ptr<logger> log)
{
    tpm_context_config config;
    config.tcti = tcti_string_config{std::move(tcti_config)};
    config.startup = startup;
    config.log = std::move(log);
    return create(std::move(config));
}

outcome<std::unique_ptr<pcr::provider>>
tpm_context::create_pcr_provider(pcr::observer* const observer)
{
    if (impl_ == nullptr || impl_->esys() == nullptr) {
        return tl::unexpected(
            error{error_category::resource_error, "TPM context does not contain a usable backend"});
    }

    return std::make_unique<detail::tpm2_esys::esys_pcr_provider>(impl_->esys(), impl_->log(),
                                                                  observer, impl_->esys_api());
}

namespace tpm2_esys {

outcome<tpm_context> create_context(owned_tcti_context tcti,
                                    const tpm_context_config::startup_mode startup,
                                    std::shared_ptr<logger> log)
{
    return detail::tpm2_esys::create_context_from_owned_tcti(
        std::move(tcti), startup, std::move(log), detail::tpm2_esys::default_esys_api());
}

} // namespace tpm2_esys

} // namespace tpmkit
