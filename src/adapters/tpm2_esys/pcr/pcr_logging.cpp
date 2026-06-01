#include "pcr_logging.h"

#include "../support/log_events.h"

#include <tpmkit/logging/logger.h>

#include <array>
#include <string>
#include <string_view>

namespace tpmkit::detail::tpm2_esys {
namespace {

template <std::size_t size>
void log_pcr_event(logger& log, const events::event_descriptor event,
                   const std::array<log_field, size>& fields)
{
    std::array<log_field, size + 3U> merged{};
    merged[0U] = {events::fields::event, event.name};
    merged[1U] = {events::fields::component, events::component_tpm2_esys};
    merged[2U] = {events::fields::outcome, events::values::success};
    for (std::size_t index = 0U; index < size; ++index) {
        merged[index + 3U] = fields[index];
    }

    log.log(log_level::info, event.message, gsl::span<const log_field>(merged));
}

} // namespace

void log_allocate_completed(logger& log, const bool allocation_success,
                            const std::size_t bank_count)
{
    const std::string allocation_success_value = allocation_success ? "true" : "false";
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::allocation_success, allocation_success_value},
        {events::fields::bank_count, bank_count_value},
    }};

    log_pcr_event(log, events::pcr_allocate_completed, fields);
}

void log_auth_policy_set(logger& log, const pcr::index index, const hash_algorithm algorithm)
{
    const std::string index_value = std::to_string(index.value());
    const std::string_view algorithm_value = hash_algorithm_name(algorithm);
    const std::array<log_field, 2U> fields{{
        {events::fields::pcr_index, index_value},
        {events::fields::policy_algorithm, algorithm_value},
    }};

    log_pcr_event(log, events::pcr_auth_policy_set, fields);
}

void log_auth_value_set(logger& log, const pcr::index index)
{
    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log_pcr_event(log, events::pcr_auth_value_set, fields);
}

void log_event_completed(logger& log, const pcr::index index, const std::size_t event_size,
                         const std::size_t bank_count)
{
    const std::string bank_count_value = std::to_string(bank_count);
    const std::string index_value = std::to_string(index.value());
    const std::string event_size_value = std::to_string(event_size);
    const std::array<log_field, 3U> fields{{
        {events::fields::bank_count, bank_count_value},
        {events::fields::event_size, event_size_value},
        {events::fields::pcr_index, index_value},
    }};

    log_pcr_event(log, events::pcr_event_completed, fields);
}

void log_extend_completed(logger& log, const pcr::index index, const std::size_t bank_count)
{
    const std::string index_value = std::to_string(index.value());
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::bank_count, bank_count_value},
        {events::fields::pcr_index, index_value},
    }};

    log_pcr_event(log, events::pcr_extend_completed, fields);
}

void log_read_completed(logger& log, const pcr::selection& selection, const std::size_t value_count)
{
    const std::string pcr_count = std::to_string(value_count);
    const std::string_view bank = hash_algorithm_name(selection.algorithm());
    const std::array<log_field, 2U> fields{{
        {events::fields::bank, bank},
        {events::fields::pcr_count, pcr_count},
    }};

    log_pcr_event(log, events::pcr_read_completed, fields);
}

void log_reset_completed(logger& log, const pcr::index index)
{
    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log_pcr_event(log, events::pcr_reset_completed, fields);
}

} // namespace tpmkit::detail::tpm2_esys
