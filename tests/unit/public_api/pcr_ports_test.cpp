#include <tpmkit/pcr_observer.h>
#include <tpmkit/pcr_provider.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace {

class test_pcr_ports final : public tpmkit::pcr_provider, public tpmkit::pcr_observer {
public:
    [[nodiscard]] tpmkit::outcome<tpmkit::pcr_allocate_result>
    allocate(gsl::span<const tpmkit::pcr_bank>) final
    {
        return tpmkit::pcr_allocate_result{true, 24U, 0U, 0U};
    }

    [[nodiscard]] tpmkit::outcome<tpmkit::pcr_event_result>
    event(tpmkit::pcr_index, gsl::span<const std::uint8_t>) final
    {
        return tpmkit::pcr_event_result{};
    }

    [[nodiscard]] tpmkit::outcome<void> extend(tpmkit::pcr_index,
                                               gsl::span<const tpmkit::pcr_digest_value>) final
    {
        return {};
    }

    void on_event(tpmkit::pcr_index, gsl::span<const std::uint8_t>,
                  const tpmkit::pcr_event_result&) noexcept final
    {}

    void on_extend(tpmkit::pcr_index, gsl::span<const tpmkit::pcr_digest_value>) noexcept final {}

    [[nodiscard]] tpmkit::outcome<tpmkit::pcr_read_result> read(const tpmkit::pcr_selection&) final
    {
        return tpmkit::pcr_read_result{
            tpmkit::pcr_selection{tpmkit::hash_algorithm::sha256}, 0U, {}};
    }

    [[nodiscard]] tpmkit::outcome<void> reset(tpmkit::pcr_index) final
    {
        return {};
    }

    [[nodiscard]] tpmkit::outcome<void> set_auth_policy(tpmkit::pcr_index, tpmkit::hash_algorithm,
                                                        gsl::span<const std::uint8_t>) final
    {
        return {};
    }

    [[nodiscard]] tpmkit::outcome<void> set_auth_value(tpmkit::pcr_index,
                                                       tpmkit::secret_buffer) final
    {
        return {};
    }
};

} // namespace

TEST(pcr_provider, is_abstract_with_virtual_destructor)
{
    // Verifies the provider port cannot be instantiated and deletes safely through base pointers.

    EXPECT_TRUE(std::is_abstract<tpmkit::pcr_provider>::value);
    EXPECT_TRUE(std::has_virtual_destructor<tpmkit::pcr_provider>::value);
}

TEST(pcr_observer, is_abstract_with_virtual_destructor)
{
    // Verifies the observer port cannot be instantiated and deletes safely through base pointers.

    EXPECT_TRUE(std::is_abstract<tpmkit::pcr_observer>::value);
    EXPECT_TRUE(std::has_virtual_destructor<tpmkit::pcr_observer>::value);
}

TEST(pcr_observer, callbacks_are_noexcept)
{
    // Verifies observer callbacks cannot propagate exceptions through the observer port.

    test_pcr_ports ports;
    const tpmkit::pcr_event_result result{};
    const auto digest_span = gsl::span<const tpmkit::pcr_digest_value>();
    const auto event_span = gsl::span<const std::uint8_t>();

    EXPECT_TRUE(noexcept(ports.on_extend(tpmkit::pcr_index::debug, digest_span)));
    EXPECT_TRUE(noexcept(ports.on_event(tpmkit::pcr_index::debug, event_span, result)));
}

TEST(pcr_ports, concrete_test_double_can_derive_from_both_interfaces)
{
    // Verifies a mock-style test double can implement the provider and observer ports together.

    test_pcr_ports ports;
    tpmkit::pcr_provider* const provider = &ports;
    tpmkit::pcr_observer* const observer = &ports;

    EXPECT_NE(provider, nullptr);
    EXPECT_NE(observer, nullptr);
}
