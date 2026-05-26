#pragma once

#include <tpmkit/pcr_observer.h>
#include <tpmkit/pcr_provider.h>

#include <gsl/span>

#include <cstdint>

typedef struct ESYS_CONTEXT ESYS_CONTEXT;

namespace tpmkit {

class logger;

namespace detail::esys {

class esys_pcr_provider final : public pcr_provider {
public:
    esys_pcr_provider(ESYS_CONTEXT* esys, logger& log, pcr_observer* observer) noexcept;

    [[nodiscard]] outcome<pcr_allocate_result> allocate(gsl::span<const pcr_bank> banks) final;
    [[nodiscard]] outcome<pcr_event_result> event(pcr_index index,
                                                  gsl::span<const std::uint8_t> event_data) final;
    [[nodiscard]] outcome<void> extend(pcr_index index,
                                       gsl::span<const pcr_digest_value> digests) final;
    [[nodiscard]] outcome<pcr_read_result> read(const pcr_selection& selection) final;
    [[nodiscard]] outcome<void> reset(pcr_index index) final;
    [[nodiscard]] outcome<void> set_auth_policy(pcr_index index, hash_algorithm policy_alg,
                                                gsl::span<const std::uint8_t> policy_digest) final;
    [[nodiscard]] outcome<void> set_auth_value(pcr_index index, secret_buffer auth) final;

private:
    ESYS_CONTEXT* esys_;
    logger& log_;
    pcr_observer* observer_;
};

} // namespace detail::esys
} // namespace tpmkit
