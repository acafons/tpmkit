#include <tpmkit/secret_buffer.h>

int tpmkit_secret_buffer_header_self_contained()
{
    const tpmkit::secret_buffer buffer;

    return buffer.empty() ? 0 : 1;
}
