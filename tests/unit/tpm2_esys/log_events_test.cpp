#include "src/adapters/tpm2_esys/support/log_events.h"

#include <gtest/gtest.h>

#include <string_view>

namespace {

namespace events = tpmkit::detail::esys::events;

struct schema_constant {
    std::string_view actual;
    std::string_view expected;
};

TEST(log_events, event_names_match_documented_schema)
{
    // Verifies ESYS event names match the documented logging schema.

    constexpr schema_constant event_names[]{
        {events::tcti_configured.name, "tpm.context.tcti_configured"},
        {events::tcti_configuring.name, "tpm.context.tcti_configuring"},
        {events::esys_initialized.name, "tpm.context.esys_initialized"},
        {events::pcr_allocate_completed.name, "tpm.pcr.allocate_completed"},
        {events::pcr_auth_policy_set.name, "tpm.pcr.auth_policy_set"},
        {events::pcr_auth_value_set.name, "tpm.pcr.auth_value_set"},
        {events::pcr_event_completed.name, "tpm.pcr.event_completed"},
        {events::pcr_extend_completed.name, "tpm.pcr.extend_completed"},
        {events::pcr_read_completed.name, "tpm.pcr.read_completed"},
        {events::pcr_reset_completed.name, "tpm.pcr.reset_completed"},
        {events::pcr_tss_error.name, "tpm.pcr.tss_error"},
        {events::startup_invoked.name, "tpm.context.startup_invoked"},
        {events::startup_completed.name, "tpm.context.startup_completed"},
        {events::finalized.name, "tpm.context.finalized"},
        {events::tss_error.name, "tpm.context.tss_error"},
    };

    for (const auto& event_name : event_names) {
        EXPECT_EQ(event_name.actual, event_name.expected);
    }
}

TEST(log_events, field_keys_match_documented_schema)
{
    // Verifies ESYS field keys match the documented logging schema.

    constexpr schema_constant field_keys[]{
        {events::fields::event, "event"},
        {events::fields::component, "component"},
        {events::fields::outcome, "outcome"},
        {events::fields::error_category, "error_category"},
        {events::fields::error_code, "error_code"},
        {events::fields::tcti_kind, "tcti_kind"},
        {events::fields::tcti_name, "tcti_name"},
        {events::fields::abi_version, "abi_version"},
        {events::fields::allocation_success, "allocation_success"},
        {events::fields::bank, "bank"},
        {events::fields::bank_count, "bank_count"},
        {events::fields::event_size, "event_size"},
        {events::fields::startup_mode, "startup_mode"},
        {events::fields::pcr_count, "pcr_count"},
        {events::fields::pcr_index, "pcr_index"},
        {events::fields::result, "result"},
        {events::fields::policy_algorithm, "policy_algorithm"},
        {events::fields::operation, "operation"},
        {events::fields::source, "source"},
        {events::fields::tss_layer, "tss_layer"},
    };

    for (const auto& field_key : field_keys) {
        EXPECT_EQ(field_key.actual, field_key.expected);
    }
}

TEST(log_events, standard_values_match_documented_schema)
{
    // Verifies shared structured values match the documented logging schema.

    constexpr schema_constant values[]{
        {events::component_tpm2_esys, "tpm2_esys"},
        {events::values::backend_error, "backend_error"},
        {events::values::failure, "failure"},
        {events::values::input_error, "input_error"},
        {events::values::resource_error, "resource_error"},
        {events::values::security_failure, "security_failure"},
        {events::values::success, "success"},
    };

    for (const auto& value : values) {
        EXPECT_EQ(value.actual, value.expected);
    }
}

} // namespace
