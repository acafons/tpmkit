#pragma once

#include "log_events.h"

#include <tpmkit/logging/logger.h>

#include <array>
#include <cstddef>
#include <string_view>

namespace tpmkit::detail::tpm2_esys {

template <std::size_t size>
void emit_outcome_record(logger& log, const log_level level, const events::event_descriptor event,
                         const std::string_view outcome,
                         const std::array<log_field, size>& fields) noexcept
{
    std::array<log_field, size + 3U> merged{};
    merged[0U] = {events::fields::event, event.name};
    merged[1U] = {events::fields::component, events::component_tpm2_esys};
    merged[2U] = {events::fields::outcome, outcome};
    for (std::size_t index = 0U; index < size; ++index) {
        merged[index + 3U] = fields[index];
    }

    log.log(level, event.message, gsl::span<const log_field>(merged));
}

template <std::size_t size>
void emit_failure_record(logger* const log, const events::event_descriptor event,
                         const std::array<log_field, size>& fields) noexcept
{
    if (log == nullptr) {
        return;
    }

    emit_outcome_record(*log, log_level::error, event, events::values::failure, fields);
}

template <std::size_t size>
void emit_success_record(logger& log, const events::event_descriptor event,
                         const std::array<log_field, size>& fields) noexcept
{
    emit_outcome_record(log, log_level::info, event, events::values::success, fields);
}

template <std::size_t size>
void emit_success_record(logger* const log, const events::event_descriptor event,
                         const std::array<log_field, size>& fields) noexcept
{
    if (log == nullptr) {
        return;
    }

    emit_success_record(*log, event, fields);
}

} // namespace tpmkit::detail::tpm2_esys
