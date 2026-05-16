#include <tpmkit/testing/recording_logger.h>

namespace tpmkit::testing {

namespace {

log_record make_record(
    const log_level level,
    const std::string_view message,
    const gsl::span<const log_field> fields)
{
    log_record record{level, std::string{message}, {}};
    record.fields.reserve(fields.size());

    for (const log_field& field : fields) {
        record.fields.emplace_back(std::string{field.key}, std::string{field.value});
    }

    return record;
}

} // namespace

void recording_logger::clear()
{
    const std::lock_guard<std::mutex> lock{mu_};
    records_.clear();
}

void recording_logger::log(
    const log_level level,
    const std::string_view message,
    const gsl::span<const log_field> fields) noexcept
{
    try {
        log_record record = make_record(level, message, fields);
        const std::lock_guard<std::mutex> lock{mu_};
        records_.emplace_back(std::move(record));
    } catch (...) {
        // The logger port is a noexcept boundary; failed captures are dropped.
    }
}

std::vector<log_record> recording_logger::snapshot() const
{
    const std::lock_guard<std::mutex> lock{mu_};
    return records_;
}

} // namespace tpmkit::testing
