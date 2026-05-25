#include <tpmkit/pcr_index.h>

#include <tpmkit/exception.h>

namespace tpmkit {

const pcr_index pcr_index::application{23U, unchecked_tag{}};
const pcr_index pcr_index::bootloader_8{8U, unchecked_tag{}};
const pcr_index pcr_index::bootloader_9{9U, unchecked_tag{}};
const pcr_index pcr_index::debug{16U, unchecked_tag{}};
const pcr_index pcr_index::drtm_17{17U, unchecked_tag{}};
const pcr_index pcr_index::drtm_18{18U, unchecked_tag{}};
const pcr_index pcr_index::drtm_19{19U, unchecked_tag{}};
const pcr_index pcr_index::drtm_20{20U, unchecked_tag{}};
const pcr_index pcr_index::drtm_21{21U, unchecked_tag{}};
const pcr_index pcr_index::drtm_22{22U, unchecked_tag{}};
const pcr_index pcr_index::firmware_0{0U, unchecked_tag{}};
const pcr_index pcr_index::firmware_1{1U, unchecked_tag{}};
const pcr_index pcr_index::firmware_2{2U, unchecked_tag{}};
const pcr_index pcr_index::firmware_3{3U, unchecked_tag{}};
const pcr_index pcr_index::firmware_4{4U, unchecked_tag{}};
const pcr_index pcr_index::firmware_5{5U, unchecked_tag{}};
const pcr_index pcr_index::firmware_6{6U, unchecked_tag{}};
const pcr_index pcr_index::firmware_7{7U, unchecked_tag{}};
const pcr_index pcr_index::ima{10U, unchecked_tag{}};
const pcr_index pcr_index::os_11{11U, unchecked_tag{}};
const pcr_index pcr_index::os_12{12U, unchecked_tag{}};
const pcr_index pcr_index::os_13{13U, unchecked_tag{}};
const pcr_index pcr_index::os_14{14U, unchecked_tag{}};
const pcr_index pcr_index::os_15{15U, unchecked_tag{}};

pcr_index::pcr_index(const std::uint32_t value)
{
    if (value > max_value) {
        throw input_validation_error{"PCR index must be in the range 0-31"};
    }

    value_ = static_cast<std::uint8_t>(value);
}

std::uint8_t pcr_index::value() const noexcept
{
    return value_;
}

bool pcr_index::operator!=(const pcr_index& other) const noexcept
{
    return !(*this == other);
}

bool pcr_index::operator<(const pcr_index& other) const noexcept
{
    return value_ < other.value_;
}

bool pcr_index::operator==(const pcr_index& other) const noexcept
{
    return value_ == other.value_;
}

} // namespace tpmkit
