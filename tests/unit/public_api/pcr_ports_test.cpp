#include <tpmkit/pcr/observer.h>
#include <tpmkit/pcr/provider.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace {

class test_pcr_ports final : public tpmkit::pcr::provider, public tpmkit::pcr::observer {
public:
    [[nodiscard]] tpmkit::outcome<tpmkit::pcr::allocate_result>
    allocate(gsl::span<const tpmkit::pcr::bank>) final
    {
        return tpmkit::pcr::allocate_result{true, 24U, 0U, 0U};
    }

    [[nodiscard]] tpmkit::outcome<tpmkit::pcr::event_result>
    event(tpmkit::pcr::index, gsl::span<const std::uint8_t>) final
    {
        return tpmkit::pcr::event_result{};
    }

    [[nodiscard]] tpmkit::outcome<void> extend(tpmkit::pcr::index,
                                               gsl::span<const tpmkit::pcr::digest_value>) final
    {
        return {};
    }

    void on_event(tpmkit::pcr::index, gsl::span<const std::uint8_t>,
                  const tpmkit::pcr::event_result&) noexcept final
    {}

    void on_extend(tpmkit::pcr::index, gsl::span<const tpmkit::pcr::digest_value>) noexcept final {}

    [[nodiscard]] tpmkit::outcome<tpmkit::pcr::read_result>
    read(const tpmkit::pcr::selection&) final
    {
        return tpmkit::pcr::read_result{
            tpmkit::pcr::selection{tpmkit::hash_algorithm::sha256}, 0U, {}};
    }

    [[nodiscard]] tpmkit::outcome<void> reset(tpmkit::pcr::index) final
    {
        return {};
    }

    [[nodiscard]] tpmkit::outcome<void> set_auth_policy(tpmkit::pcr::index, tpmkit::hash_algorithm,
                                                        gsl::span<const std::uint8_t>) final
    {
        return {};
    }

    [[nodiscard]] tpmkit::outcome<void> set_auth_value(tpmkit::pcr::index,
                                                       tpmkit::secret_buffer) final
    {
        return {};
    }
};

} // namespace

TEST(pcr_provider, is_abstract_with_virtual_destructor)
{
    // Verifies the provider port cannot be instantiated and deletes safely through base pointers.

    EXPECT_TRUE(std::is_abstract<tpmkit::pcr::provider>::value);
    EXPECT_TRUE(std::has_virtual_destructor<tpmkit::pcr::provider>::value);
}

TEST(pcr_observer, is_abstract_with_virtual_destructor)
{
    // Verifies the observer port cannot be instantiated and deletes safely through base pointers.

    EXPECT_TRUE(std::is_abstract<tpmkit::pcr::observer>::value);
    EXPECT_TRUE(std::has_virtual_destructor<tpmkit::pcr::observer>::value);
}

TEST(pcr_observer, callbacks_are_noexcept)
{
    // Verifies observer callbacks cannot propagate exceptions through the observer port.

    test_pcr_ports ports;
    const tpmkit::pcr::event_result result{};
    const auto digest_span = gsl::span<const tpmkit::pcr::digest_value>();
    const auto event_span = gsl::span<const std::uint8_t>();

    EXPECT_TRUE(noexcept(ports.on_extend(tpmkit::pcr::index::debug, digest_span)));
    EXPECT_TRUE(noexcept(ports.on_event(tpmkit::pcr::index::debug, event_span, result)));
}

TEST(pcr_ports, concrete_test_double_can_derive_from_both_interfaces)
{
    // Verifies a mock-style test double can implement the provider and observer ports together.

    test_pcr_ports ports;
    tpmkit::pcr::provider* const provider = &ports;
    tpmkit::pcr::observer* const observer = &ports;

    EXPECT_NE(provider, nullptr);
    EXPECT_NE(observer, nullptr);
}
