#include <tpmkit/testing/in_memory_pcr_observer.h>

#include <algorithm>
#include <iterator>

namespace tpmkit::testing {

namespace {

[[nodiscard]] pcr_measurement_record
make_event_record(const pcr_index index, const gsl::span<const std::uint8_t> event_data,
                  const pcr_event_result& result)
{
    return pcr_measurement_record{index, pcr_measurement_operation::event, result.digests,
                                  std::vector<std::uint8_t>{event_data.begin(), event_data.end()}};
}

[[nodiscard]] pcr_measurement_record
make_extend_record(const pcr_index index, const gsl::span<const pcr_digest_value> digests)
{
    return pcr_measurement_record{index,
                                  pcr_measurement_operation::extend,
                                  std::vector<pcr_digest_value>{digests.begin(), digests.end()},
                                  {}};
}

} // namespace

void in_memory_pcr_observer::clear()
{
    records_.clear();
}

std::size_t in_memory_pcr_observer::count() const noexcept
{
    return records_.size();
}

const std::vector<pcr_measurement_record>& in_memory_pcr_observer::entries() const noexcept
{
    return records_;
}

std::vector<pcr_measurement_record>
in_memory_pcr_observer::entries_by_index(const pcr_index index) const
{
    std::vector<pcr_measurement_record> matches;
    std::copy_if(records_.begin(), records_.end(), std::back_inserter(matches),
                 [index](const pcr_measurement_record& record) { return record.index == index; });

    return matches;
}

void in_memory_pcr_observer::on_event(const pcr_index index,
                                      const gsl::span<const std::uint8_t> event_data,
                                      const pcr_event_result& result) noexcept
{
    try {
        records_.emplace_back(make_event_record(index, event_data, result));
    } catch (...) {
        // The observer port is a noexcept boundary; failed captures are dropped.
    }
}

void in_memory_pcr_observer::on_extend(const pcr_index index,
                                       const gsl::span<const pcr_digest_value> digests) noexcept
{
    try {
        records_.emplace_back(make_extend_record(index, digests));
    } catch (...) {
        // The observer port is a noexcept boundary; failed captures are dropped.
    }
}

} // namespace tpmkit::testing
