#include "src/adapters/tpm2_esys/log_events.h"

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
        {events::tcti_configured, "tpm.context.tcti_configured"},
        {events::tcti_configuring, "tpm.context.tcti_configuring"},
        {events::esys_initialized, "tpm.context.esys_initialized"},
        {events::startup_invoked, "tpm.context.startup_invoked"},
        {events::startup_completed, "tpm.context.startup_completed"},
        {events::finalized, "tpm.context.finalized"},
        {events::tss_error, "tpm.context.tss_error"},
    };

    for (const auto& event_name : event_names) {
        EXPECT_EQ(event_name.actual, event_name.expected);
    }
}

TEST(log_events, field_keys_match_documented_schema)
{
    // Verifies ESYS field keys match the documented logging schema.

    constexpr schema_constant field_keys[]{
        {events::fields::tcti_kind, "tcti_kind"},
        {events::fields::tcti_name, "tcti_name"},
        {events::fields::abi_version, "abi_version"},
        {events::fields::startup_mode, "startup_mode"},
        {events::fields::result, "result"},
        {events::fields::operation, "operation"},
        {events::fields::tss_rc_hex, "tss_rc_hex"},
        {events::fields::tss_layer, "tss_layer"},
    };

    for (const auto& field_key : field_keys) {
        EXPECT_EQ(field_key.actual, field_key.expected);
    }
}

} // namespace
