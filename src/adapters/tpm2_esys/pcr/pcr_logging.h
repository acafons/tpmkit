#pragma once

#include <tpmkit/hash_algorithm.h>
#include <tpmkit/pcr/index.h>
#include <tpmkit/pcr/selection.h>

#include <cstddef>

namespace tpmkit {

class logger;

namespace detail::esys {

void log_allocate_completed(logger& log, bool allocation_success, std::size_t bank_count);

void log_auth_policy_set(logger& log, pcr::index index, hash_algorithm algorithm);

void log_auth_value_set(logger& log, pcr::index index);

void log_event_completed(logger& log, pcr::index index, std::size_t event_size,
                         std::size_t bank_count);

void log_extend_completed(logger& log, pcr::index index, std::size_t bank_count);

void log_read_completed(logger& log, const pcr::selection& selection, std::size_t value_count);

void log_reset_completed(logger& log, pcr::index index);

} // namespace detail::esys
} // namespace tpmkit
