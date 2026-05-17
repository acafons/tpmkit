#include <tpmkit/exception.h>

int tpmkit_exception_header_smoke()
{
    const tpmkit::tpmkit_error error{"exception"};

    return error.what()[0];
}
