#include <tpmkit/encoding/hex.h>

int tpmkit_encoding_hex_header_self_contained()
{
    const std::vector<std::uint8_t> bytes{0xabU};
    const tpmkit::outcome<std::vector<std::uint8_t>> decoded = tpmkit::encoding::decode_hex("ab");

    return decoded.has_value() &&
                   tpmkit::encoding::encode_hex(gsl::span<const std::uint8_t>{bytes}) == "ab"
               ? 0
               : 1;
}
