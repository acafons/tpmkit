#include <tpmkit/tpm_context.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <utility>

namespace {

void noop_tcti_deleter(TSS2_TCTI_CONTEXT*) noexcept {}

struct tcti_kind_visitor {
    const char* operator()(const tpmkit::tcti_owned_handle&) const noexcept
    {
        return "owned";
    }

    const char* operator()(const tpmkit::tcti_string_config&) const noexcept
    {
        return "string";
    }
};

static_assert(
    std::is_invocable_r<const char*, tcti_kind_visitor, const tpmkit::tcti_string_config&>::value,
    "visitor must accept string TCTI configs");
static_assert(
    std::is_invocable_r<const char*, tcti_kind_visitor, const tpmkit::tcti_owned_handle&>::value,
    "visitor must accept owned TCTI handles");

TEST(tpm_context_config, defaults_to_empty_string_tcti_and_clear_startup)
{
    // Verifies the default TPM context config uses the documented defaults.

    const tpmkit::tpm_context_config config;

    ASSERT_TRUE(std::holds_alternative<tpmkit::tcti_string_config>(config.tcti));
    EXPECT_TRUE(std::get<tpmkit::tcti_string_config>(config.tcti).config.empty());
    EXPECT_EQ(config.startup, tpmkit::tpm_context_config::startup_mode::clear);
    EXPECT_EQ(config.log, nullptr);
}

TEST(tpm_context_config, visits_string_tcti_alternative)
{
    // Verifies string TCTI configs are visitable through the config variant.

    tpmkit::tpm_context_config config;
    config.tcti = tpmkit::tcti_string_config{"mssim:host=localhost,port=2321"};

    const char* const kind = std::visit(tcti_kind_visitor{}, config.tcti);

    EXPECT_STREQ(kind, "string");
}

TEST(tpm_context_config, visits_owned_tcti_alternative)
{
    // Verifies owned TCTI configs are visitable through the config variant.

    tpmkit::tpm_context_config config;
    config.tcti =
        tpmkit::tcti_owned_handle{std::unique_ptr<TSS2_TCTI_CONTEXT, void (*)(TSS2_TCTI_CONTEXT*)>(
            nullptr, noop_tcti_deleter)};

    const char* const kind = std::visit(tcti_kind_visitor{}, config.tcti);

    EXPECT_STREQ(kind, "owned");
}

TEST(tpm_context, is_move_only)
{
    // Verifies tpm_context preserves move-only ownership semantics.

    EXPECT_TRUE(std::is_move_constructible<tpmkit::tpm_context>::value);
    EXPECT_TRUE(std::is_move_assignable<tpmkit::tpm_context>::value);
    EXPECT_FALSE(std::is_copy_constructible<tpmkit::tpm_context>::value);
    EXPECT_FALSE(std::is_copy_assignable<tpmkit::tpm_context>::value);
}

} // namespace
