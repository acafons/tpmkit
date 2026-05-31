#include <tpmkit/tpm_context.h>

#include <type_traits>

static_assert(std::is_move_constructible<tpmkit::tpm_context>::value,
              "tpm_context must move construct");
static_assert(std::is_move_assignable<tpmkit::tpm_context>::value, "tpm_context must move assign");
static_assert(!std::is_copy_constructible<tpmkit::tpm_context>::value,
              "tpm_context must not copy construct");
static_assert(!std::is_copy_assignable<tpmkit::tpm_context>::value,
              "tpm_context must not copy assign");
static_assert(std::is_same<decltype(tpmkit::tpm_context::create("mssim:host=localhost,port=2321")),
                           tpmkit::outcome<tpmkit::tpm_context>>::value,
              "tpm_context string TCTI overload must return outcome<tpm_context>");

int tpmkit_tpm_context_header_smoke()
{
    tpmkit::tpm_context_config config;

    return config.tcti.config.empty() ? 0 : 1;
}
