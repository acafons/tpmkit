#include <tpmkit/testing/fake_tcti.h>

#include <gtest/gtest.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_tcti.h>
#include <tss2/tss2_tpm2_types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr std::array<std::uint8_t, 1> command{{0x80U}};

TSS2_TCTI_CONTEXT_COMMON_V1* common(TSS2_TCTI_CONTEXT* const context)
{
    return reinterpret_cast<TSS2_TCTI_CONTEXT_COMMON_V1*>(context);
}

TSS2_RC cancel(tpmkit::testing::fake_tcti& fake)
{
    return common(fake.handle())->cancel(fake.handle());
}

TSS2_RC get_poll_handles(tpmkit::testing::fake_tcti& fake, std::size_t& poll_handle_count)
{
    return common(fake.handle())->getPollHandles(fake.handle(), nullptr, &poll_handle_count);
}

TSS2_RC receive(tpmkit::testing::fake_tcti& fake, std::size_t& response_size,
                std::uint8_t* const response)
{
    return common(fake.handle())
        ->receive(fake.handle(), &response_size, response, TSS2_TCTI_TIMEOUT_NONE);
}

TSS2_RC set_locality(tpmkit::testing::fake_tcti& fake)
{
    return common(fake.handle())->setLocality(fake.handle(), 0U);
}

TSS2_RC transmit(tpmkit::testing::fake_tcti& fake)
{
    return common(fake.handle())->transmit(fake.handle(), command.size(), command.data());
}

} // namespace

TEST(fake_tcti, push_response_increments_pending_responses)
{
    // Verifies queued responses are counted as pending.

    tpmkit::testing::fake_tcti fake;

    fake.push_response({0x01U, 0x02U});

    EXPECT_EQ(fake.pending_responses(), 1U);
}

TEST(fake_tcti, response_and_failure_are_received_in_fifo_order)
{
    // Verifies scripted responses and failures are consumed in FIFO order.

    tpmkit::testing::fake_tcti fake;
    fake.push_response({0x01U, 0x02U});
    fake.push_failure(TPM2_RC_INITIALIZE);

    ASSERT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    std::array<std::uint8_t, 4> response{};
    std::size_t response_size = response.size();
    EXPECT_EQ(receive(fake, response_size, response.data()), TSS2_RC_SUCCESS);
    EXPECT_EQ(response_size, 2U);
    EXPECT_EQ(response[0], 0x01U);
    EXPECT_EQ(response[1], 0x02U);

    ASSERT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    response_size = response.size();
    EXPECT_EQ(receive(fake, response_size, response.data()), TPM2_RC_INITIALIZE);
    EXPECT_EQ(fake.pending_responses(), 0U);
}

TEST(fake_tcti, transmit_underflow_returns_deterministic_io_error)
{
    // Verifies transmit without a queued response fails deterministically.

    tpmkit::testing::fake_tcti fake;

    EXPECT_EQ(transmit(fake), TSS2_TCTI_RC_IO_ERROR);
}

TEST(fake_tcti, receive_underflow_returns_deterministic_io_error)
{
    // Verifies receive without a queued response fails deterministically.

    tpmkit::testing::fake_tcti fake;
    std::array<std::uint8_t, 4> response{};
    std::size_t response_size = response.size();

    EXPECT_EQ(receive(fake, response_size, response.data()), TSS2_TCTI_RC_IO_ERROR);
}

TEST(fake_tcti, receive_size_query_does_not_consume_response)
{
    // Verifies receive size queries leave the queued response available.

    tpmkit::testing::fake_tcti fake;
    fake.push_response({0x01U, 0x02U, 0x03U});
    ASSERT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    std::size_t response_size = 0U;

    EXPECT_EQ(receive(fake, response_size, nullptr), TSS2_RC_SUCCESS);
    EXPECT_EQ(response_size, 3U);
    EXPECT_EQ(fake.pending_responses(), 1U);

    std::array<std::uint8_t, 3> response{};
    response_size = response.size();
    EXPECT_EQ(receive(fake, response_size, response.data()), TSS2_RC_SUCCESS);
    EXPECT_EQ(fake.pending_responses(), 0U);
}

TEST(fake_tcti, response_factory_can_reenter_fake_inspection_without_deadlock)
{
    // Verifies response factories run outside the fake TCTI mutex.

    tpmkit::testing::fake_tcti fake;
    fake.push_response_factory([&fake](const std::vector<std::uint8_t>& transmitted) {
        EXPECT_EQ(fake.pending_responses(), 1U);
        const auto commands = fake.transmitted_commands();
        EXPECT_EQ(commands.size(), 1U);
        EXPECT_EQ(commands.front(), transmitted);
        return std::vector<std::uint8_t>{0x04U, 0x05U};
    });
    ASSERT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    std::array<std::uint8_t, 4> response{};
    std::size_t response_size = response.size();

    EXPECT_EQ(receive(fake, response_size, response.data()), TSS2_RC_SUCCESS);

    EXPECT_EQ(response_size, 2U);
    EXPECT_EQ(response[0], 0x04U);
    EXPECT_EQ(response[1], 0x05U);
    EXPECT_EQ(fake.pending_responses(), 0U);
}

TEST(fake_tcti, transmits_observed_counts_transmit_calls)
{
    // Verifies transmit callbacks are counted.

    tpmkit::testing::fake_tcti fake;
    fake.push_response({0x01U});
    fake.push_response({0x02U});

    EXPECT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    EXPECT_EQ(transmit(fake), TSS2_RC_SUCCESS);

    EXPECT_EQ(fake.transmits_observed(), 2U);
}

TEST(fake_tcti, finalizes_observed_counts_finalize_callbacks)
{
    // Verifies finalize callbacks are counted.

    tpmkit::testing::fake_tcti fake;

    common(fake.handle())->finalize(fake.handle());

    EXPECT_EQ(fake.finalizes_observed(), 1U);
}

TEST(fake_tcti, pushed_failure_is_returned_by_receive_after_transmit)
{
    // Verifies queued failures surface on the matching receive call.

    tpmkit::testing::fake_tcti fake;
    fake.push_failure(TPM2_RC_INITIALIZE);

    ASSERT_EQ(transmit(fake), TSS2_RC_SUCCESS);
    std::array<std::uint8_t, 4> response{};
    std::size_t response_size = response.size();

    EXPECT_EQ(receive(fake, response_size, response.data()), TPM2_RC_INITIALIZE);
}

TEST(fake_tcti, pushed_transmit_failure_is_returned_by_transmit)
{
    // Verifies queued transmit failures surface on the matching transmit call.

    tpmkit::testing::fake_tcti fake;
    fake.push_transmit_failure(TSS2_TCTI_RC_IO_ERROR);

    EXPECT_EQ(transmit(fake), TSS2_TCTI_RC_IO_ERROR);
    EXPECT_EQ(fake.pending_responses(), 0U);
}

TEST(fake_tcti, move_transfers_a_usable_handle_to_the_new_owner)
{
    // Verifies moving fake_tcti transfers a usable TCTI handle.

    static_assert(std::is_move_constructible<tpmkit::testing::fake_tcti>::value,
                  "fake_tcti must be move constructible");
    static_assert(std::is_move_assignable<tpmkit::testing::fake_tcti>::value,
                  "fake_tcti must be move assignable");
    static_assert(!std::is_copy_constructible<tpmkit::testing::fake_tcti>::value,
                  "fake_tcti must not be copy constructible");
    static_assert(!std::is_copy_assignable<tpmkit::testing::fake_tcti>::value,
                  "fake_tcti must not be copy assignable");

    tpmkit::testing::fake_tcti fake;
    fake.push_response({0x01U});
    TSS2_TCTI_CONTEXT* const original_handle = fake.handle();

    tpmkit::testing::fake_tcti moved{std::move(fake)};

    EXPECT_EQ(moved.handle(), original_handle);
    EXPECT_EQ(transmit(moved), TSS2_RC_SUCCESS);
}

TEST(fake_tcti, esys_initialize_accepts_handle)
{
    // Verifies ESYS accepts the fake TCTI handle shape.

    tpmkit::testing::fake_tcti fake;
    ESYS_CONTEXT* esys_context = nullptr;

    const TSS2_RC rc = Esys_Initialize(&esys_context, fake.handle(), nullptr);

    EXPECT_EQ(rc, TSS2_RC_SUCCESS);
    Esys_Finalize(&esys_context);
}

TEST(fake_tcti, finalize_function_pointer_can_run_without_owner_state)
{
    // Verifies copied finalize callbacks tolerate missing owner state.

    struct copied_context {
        TSS2_TCTI_CONTEXT_COMMON_V1 common;
        void* owner;
    };

    copied_context context{};
    {
        tpmkit::testing::fake_tcti fake;
        context.common = *reinterpret_cast<TSS2_TCTI_CONTEXT_COMMON_V1*>(fake.handle());
    }

    ASSERT_NE(context.common.finalize, nullptr);
    context.common.finalize(reinterpret_cast<TSS2_TCTI_CONTEXT*>(&context));
    EXPECT_EQ(context.owner, nullptr);
}

TEST(fake_tcti, poll_cancel_and_locality_dispatch_are_noops)
{
    // Verifies optional TCTI callbacks succeed as no-ops.

    tpmkit::testing::fake_tcti fake;
    std::size_t poll_handle_count = 1U;

    EXPECT_EQ(cancel(fake), TSS2_RC_SUCCESS);
    EXPECT_EQ(set_locality(fake), TSS2_RC_SUCCESS);
    EXPECT_EQ(get_poll_handles(fake, poll_handle_count), TSS2_RC_SUCCESS);
    EXPECT_EQ(poll_handle_count, 0U);
}
