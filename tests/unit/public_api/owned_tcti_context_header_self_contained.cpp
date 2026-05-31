#include <tpmkit/tpm2_esys/owned_tcti_context.h>

int tpmkit_owned_tcti_context_header_self_contained()
{
    tpmkit::tpm2_esys::owned_tcti_context context;
    return context.handle == nullptr ? 0 : 1;
}
