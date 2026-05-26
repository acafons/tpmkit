#include <tpmkit/pcr/result_types.h>

namespace tpmkit {

bool operator==(const pcr_value& lhs, const pcr_value& rhs)
{
    return lhs.index == rhs.index && lhs.digest == rhs.digest;
}

bool operator!=(const pcr_value& lhs, const pcr_value& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const pcr_read_result& lhs, const pcr_read_result& rhs)
{
    return lhs.actual_selection == rhs.actual_selection &&
           lhs.update_counter == rhs.update_counter && lhs.values == rhs.values;
}

bool operator!=(const pcr_read_result& lhs, const pcr_read_result& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const pcr_event_result& lhs, const pcr_event_result& rhs)
{
    return lhs.digests == rhs.digests;
}

bool operator!=(const pcr_event_result& lhs, const pcr_event_result& rhs)
{
    return !(lhs == rhs);
}

bool operator==(const pcr_allocate_result& lhs, const pcr_allocate_result& rhs) noexcept
{
    return lhs.allocation_success == rhs.allocation_success && lhs.max_pcr == rhs.max_pcr &&
           lhs.size_needed == rhs.size_needed && lhs.size_available == rhs.size_available;
}

bool operator!=(const pcr_allocate_result& lhs, const pcr_allocate_result& rhs) noexcept
{
    return !(lhs == rhs);
}

} // namespace tpmkit
