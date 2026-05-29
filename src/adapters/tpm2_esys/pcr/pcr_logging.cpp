#include "pcr_logging.h"

#include "../support/log_events.h"

#include <tpmkit/logging/logger.h>

#include <array>
#include <string>
#include <string_view>

namespace tpmkit::detail::esys {

void log_allocate_completed(logger& log, const bool allocation_success,
                            const std::size_t bank_count)
{
    const std::string allocation_success_value = allocation_success ? "true" : "false";
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::allocation_success, allocation_success_value},
        {events::fields::bank_count, bank_count_value},
    }};

    log.log(log_level::info, events::pcr_allocate_completed, gsl::span<const log_field>(fields));
}

void log_auth_policy_set(logger& log, const pcr::index index, const hash_algorithm algorithm)
{
    const std::string index_value = std::to_string(index.value());
    const std::string_view algorithm_value = hash_algorithm_name(algorithm);
    const std::array<log_field, 2U> fields{{
        {events::fields::pcr_index, index_value},
        {events::fields::policy_algorithm, algorithm_value},
    }};

    log.log(log_level::info, events::pcr_auth_policy_set, gsl::span<const log_field>(fields));
}

void log_auth_value_set(logger& log, const pcr::index index)
{
    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log.log(log_level::info, events::pcr_auth_value_set, gsl::span<const log_field>(fields));
}

void log_event_completed(logger& log, const pcr::index index, const std::size_t event_size)
{
    const std::string index_value = std::to_string(index.value());
    const std::string event_size_value = std::to_string(event_size);
    const std::array<log_field, 2U> fields{{
        {events::fields::event_size, event_size_value},
        {events::fields::pcr_index, index_value},
    }};

    log.log(log_level::info, events::pcr_event_completed, gsl::span<const log_field>(fields));
}

void log_extend_completed(logger& log, const pcr::index index, const std::size_t bank_count)
{
    const std::string index_value = std::to_string(index.value());
    const std::string bank_count_value = std::to_string(bank_count);
    const std::array<log_field, 2U> fields{{
        {events::fields::bank_count, bank_count_value},
        {events::fields::pcr_index, index_value},
    }};

    log.log(log_level::info, events::pcr_extend_completed, gsl::span<const log_field>(fields));
}

void log_read_completed(logger& log, const pcr::selection& selection, const std::size_t value_count)
{
    const std::string pcr_count = std::to_string(value_count);
    const std::string_view bank = hash_algorithm_name(selection.algorithm());
    const std::array<log_field, 2U> fields{{
        {events::fields::bank, bank},
        {events::fields::pcr_count, pcr_count},
    }};

    log.log(log_level::info, events::pcr_read_completed, gsl::span<const log_field>(fields));
}

void log_reset_completed(logger& log, const pcr::index index)
{
    const std::string index_value = std::to_string(index.value());
    const std::array<log_field, 1U> fields{{
        {events::fields::pcr_index, index_value},
    }};

    log.log(log_level::info, events::pcr_reset_completed, gsl::span<const log_field>(fields));
}

} // namespace tpmkit::detail::esys
