#include "src/adapters/tpm2_esys/tcti_loader.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace {

std::string swtpm_tcti()
{
    const char* const configured = std::getenv("TPMKIT_SWTPM_TCTI");
    if (configured != nullptr) {
        return std::string{configured};
    }

    return "tabrmd:bus_type=system";
}

TEST(tcti_loader_integration, loads_configured_simulator_tcti)
{
    // Verifies the TCTI loader opens a configured simulator TCTI.

    const tpmkit::tcti_string_config config{swtpm_tcti()};

    auto result = tpmkit::detail::esys::load_tcti(config, nullptr);

    ASSERT_TRUE(result.has_value()) << result.error().message;
    EXPECT_NE(result.value().get(), nullptr);
}

} // namespace
