#pragma once

/**
 * @file tpmkit/esys_pcr_provider.h
 * @brief Public factory for wiring PCR operations to an ESYS TPM context.
 *
 * Declares the composition-root entry point for creating a PCR provider backed
 * by the TPM2 TSS ESYS adapter while keeping ESYS types out of the public API.
 */

#include <tpmkit/api.h>
#include <tpmkit/pcr_observer.h>
#include <tpmkit/pcr_provider.h>
#include <tpmkit/result.h>
#include <tpmkit/tpm_context.h>

#include <memory>

namespace tpmkit {

class logger;

/**
 * @brief Create an ESYS-backed PCR provider from an existing TPM context.
 *
 * The returned provider borrows the ESYS handle owned by `ctx` and implements
 * PCR operations through the `pcr_provider` port. The optional `observer` is
 * called after successful PCR Extend and PCR Event operations. The optional
 * `log` receives adapter records; when null, a no-op logger is used.
 *
 * @param[in,out] ctx TPM context that owns the borrowed ESYS handle. It must
 *                    remain alive and must not be moved while the returned
 *                    provider is alive.
 * @param[in] observer Optional non-owning PCR observer. It may be null.
 * @param[in] log Optional non-owning logger. Null selects the library no-op
 *                logger.
 * @return On success, an owning pointer to the PCR provider port. On failure,
 *         returns `resource_error` when `ctx` does not contain a usable ESYS
 *         handle.
 * @note The returned `pcr_provider` must not outlive `ctx`, `observer`, or
 *       `log` when those optional dependencies are supplied.
 * @thread_safety Thread-compatible. A shared `ctx` or provider requires
 *        external synchronization.
 * @exception_safety Strong; allocation failure may throw and failed creation
 *        does not modify `ctx`.
 * @since v0.1
 */
[[nodiscard]] TPMKIT_API outcome<std::unique_ptr<pcr_provider>>
create_esys_pcr_provider(tpm_context& ctx, pcr_observer* observer = nullptr, logger* log = nullptr);

} // namespace tpmkit
