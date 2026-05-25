#include <tpmkit/testing/in_memory_pcr_observer.h>

int tpmkit_in_memory_pcr_observer_header_self_contained()
{
    tpmkit::testing::in_memory_pcr_observer observer;
    return observer.count() == 0U ? 0 : 1;
}
