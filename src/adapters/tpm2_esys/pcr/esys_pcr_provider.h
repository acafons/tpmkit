#pragma once

#include "../support/esys_api.h"

#include <tpmkit/pcr/observer.h>
#include <tpmkit/pcr/provider.h>

#include <gsl/span>

#include <cstdint>

typedef struct ESYS_CONTEXT ESYS_CONTEXT;

namespace tpmkit {

class logger;

namespace detail::tpm2_esys {

class esys_pcr_provider final : public pcr::provider {
public:
    esys_pcr_provider(ESYS_CONTEXT* esys, logger& log, pcr::observer* observer) noexcept;
    esys_pcr_provider(ESYS_CONTEXT* esys, logger& log, pcr::observer* observer,
                      const esys_api& api) noexcept;

    [[nodiscard]] outcome<pcr::allocate_result> allocate(gsl::span<const pcr::bank> banks) final;
    [[nodiscard]] outcome<pcr::event_result> event(pcr::index index,
                                                   gsl::span<const std::uint8_t> event_data) final;
    [[nodiscard]] outcome<void> extend(pcr::index index,
                                       gsl::span<const pcr::digest_value> digests) final;
    [[nodiscard]] outcome<pcr::read_result> read(const pcr::selection& selection) final;
    [[nodiscard]] outcome<void> reset(pcr::index index) final;
    [[nodiscard]] outcome<void> set_auth_policy(pcr::index index, hash_algorithm policy_alg,
                                                gsl::span<const std::uint8_t> policy_digest) final;
    [[nodiscard]] outcome<void> set_auth_value(pcr::index index, secret_buffer auth) final;

private:
    const esys_api& api_;
    ESYS_CONTEXT* esys_;
    logger& log_;
    pcr::observer* observer_;
};

} // namespace detail::tpm2_esys
} // namespace tpmkit
