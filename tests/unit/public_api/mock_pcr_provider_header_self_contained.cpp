#include <tpmkit/testing/mock_pcr_provider.h>

int tpmkit_mock_pcr_provider_header_self_contained()
{
    tpmkit::testing::mock_pcr_provider provider;
    return provider.read_call_count() == 0U ? 0 : 1;
}
