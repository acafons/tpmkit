#include <tpmkit/pcr/index.h>

#include <tpmkit/exception.h>

#include <set>

namespace {

void validate_index_range(const std::uint32_t first, const std::uint32_t count)
{
    if (count == 0U) {
        return;
    }

    constexpr std::uint32_t max_index = tpmkit::pcr::index::max_value;
    if (first > max_index || count > (max_index + 1U - first)) {
        throw tpmkit::input_validation_error{"PCR index range must be in the range 0-31"};
    }
}

} // namespace

namespace tpmkit::pcr {

const index index::application{23U, unchecked_tag{}};
const index index::bootloader_8{8U, unchecked_tag{}};
const index index::bootloader_9{9U, unchecked_tag{}};
const index index::debug{16U, unchecked_tag{}};
const index index::drtm_17{17U, unchecked_tag{}};
const index index::drtm_18{18U, unchecked_tag{}};
const index index::drtm_19{19U, unchecked_tag{}};
const index index::drtm_20{20U, unchecked_tag{}};
const index index::drtm_21{21U, unchecked_tag{}};
const index index::drtm_22{22U, unchecked_tag{}};
const index index::firmware_0{0U, unchecked_tag{}};
const index index::firmware_1{1U, unchecked_tag{}};
const index index::firmware_2{2U, unchecked_tag{}};
const index index::firmware_3{3U, unchecked_tag{}};
const index index::firmware_4{4U, unchecked_tag{}};
const index index::firmware_5{5U, unchecked_tag{}};
const index index::firmware_6{6U, unchecked_tag{}};
const index index::firmware_7{7U, unchecked_tag{}};
const index index::ima{10U, unchecked_tag{}};
const index index::os_11{11U, unchecked_tag{}};
const index index::os_12{12U, unchecked_tag{}};
const index index::os_13{13U, unchecked_tag{}};
const index index::os_14{14U, unchecked_tag{}};
const index index::os_15{15U, unchecked_tag{}};

index::index(const std::uint32_t value)
{
    if (value > max_value) {
        throw input_validation_error{"PCR index must be in the range 0-31"};
    }

    value_ = static_cast<std::uint8_t>(value);
}

std::uint8_t index::value() const noexcept
{
    return value_;
}

bool index::operator!=(const index& other) const noexcept
{
    return !(*this == other);
}

bool index::operator<(const index& other) const noexcept
{
    return value_ < other.value_;
}

bool index::operator==(const index& other) const noexcept
{
    return value_ == other.value_;
}

std::set<index> make_index_range(const std::uint32_t first, const std::uint32_t count)
{
    validate_index_range(first, count);

    std::set<index> indices;
    for (std::uint32_t offset = 0U; offset < count; ++offset) {
        indices.insert(index{first + offset});
    }

    return indices;
}

} // namespace tpmkit::pcr
