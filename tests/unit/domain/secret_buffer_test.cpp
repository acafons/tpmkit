#include <tpmkit/secret_buffer.h>

#include "domain/secret_buffer_testing.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace {

struct observer_state {
    bool lock_attempted = false;
    bool locked = false;
    bool unlocked = false;
    std::size_t lock_size = 0U;
    std::vector<std::uint8_t> after_cleanse;
    std::vector<std::uint8_t> before_cleanse;
};

observer_state* active_state = nullptr;

void observe_cleanse(const std::uint8_t* const before, const std::uint8_t* const after,
                     const std::size_t size) noexcept
{
    active_state->before_cleanse.assign(before, before + size);
    active_state->after_cleanse.assign(after, after + size);
}

void observe_lock_attempt(const std::size_t size, const bool locked) noexcept
{
    active_state->lock_attempted = true;
    active_state->locked = locked;
    active_state->lock_size = size;
}

void observe_unlock(const std::size_t size) noexcept
{
    active_state->unlocked = active_state->lock_size == size;
}

class lifecycle_observer_scope final {
public:
    explicit lifecycle_observer_scope(observer_state& state) noexcept
    {
        active_state = &state;
        tpmkit::detail::secret_buffer_testing::set_lifecycle_observer(&observer_);
    }

    ~lifecycle_observer_scope() noexcept
    {
        tpmkit::detail::secret_buffer_testing::set_lifecycle_observer(nullptr);
        active_state = nullptr;
    }

    lifecycle_observer_scope(const lifecycle_observer_scope& other) = delete;
    lifecycle_observer_scope& operator=(const lifecycle_observer_scope& other) = delete;
    lifecycle_observer_scope(lifecycle_observer_scope&& other) noexcept = delete;
    lifecycle_observer_scope& operator=(lifecycle_observer_scope&& other) noexcept = delete;

private:
    tpmkit::detail::secret_buffer_testing::lifecycle_observer observer_{
        &observe_cleanse,
        &observe_lock_attempt,
        &observe_unlock,
    };
};

[[nodiscard]] bool memory_locking_should_succeed(const std::size_t size) noexcept
{
#if defined(_WIN32)
    static_cast<void>(size);
    return true;
#else
    rlimit limit{};
    if (getrlimit(RLIMIT_MEMLOCK, &limit) != 0) {
        return false;
    }

    return limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur >= size;
#endif
}

TEST(secret_buffer, default_constructs_empty)
{
    // Verifies default construction creates an empty, usable buffer.

    const tpmkit::secret_buffer buffer;

    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0U);
    EXPECT_TRUE(buffer.view().empty());
}

TEST(secret_buffer, constructs_from_vector_and_exposes_read_only_view)
{
    // Verifies construction owns bytes and exposes them through a const span.

    const std::vector<std::uint8_t> expected{0x10U, 0x20U, 0x30U};

    const tpmkit::secret_buffer buffer{expected};

    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(buffer.size(), expected.size());
    EXPECT_TRUE(
        std::equal(buffer.view().begin(), buffer.view().end(), expected.begin(), expected.end()));
}

TEST(secret_buffer, move_construction_transfers_ownership)
{
    // Verifies move construction transfers bytes and leaves the source empty.

    const std::vector<std::uint8_t> expected{0x01U, 0x02U, 0x03U};
    tpmkit::secret_buffer source{expected};

    const tpmkit::secret_buffer moved{std::move(source)};

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.size(), 0U);
    ASSERT_EQ(moved.size(), expected.size());
    EXPECT_TRUE(
        std::equal(moved.view().begin(), moved.view().end(), expected.begin(), expected.end()));
}

TEST(secret_buffer, move_assignment_transfers_ownership_and_clears_previous_content)
{
    // Verifies move assignment clears existing bytes before accepting new ownership.

    const std::vector<std::uint8_t> previous{0xA0U, 0xA1U, 0xA2U};
    const std::vector<std::uint8_t> replacement{0xB0U, 0xB1U};
    observer_state state;
    lifecycle_observer_scope observer{state};
    tpmkit::secret_buffer target{previous};
    tpmkit::secret_buffer source{replacement};

    target = std::move(source);

    EXPECT_EQ(state.before_cleanse, previous);
    ASSERT_EQ(state.after_cleanse.size(), previous.size());
    EXPECT_TRUE(std::all_of(state.after_cleanse.begin(), state.after_cleanse.end(),
                            [](const std::uint8_t value) { return value == 0U; }));
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(target.size(), replacement.size());
    EXPECT_TRUE(std::equal(target.view().begin(), target.view().end(), replacement.begin(),
                           replacement.end()));
}

TEST(secret_buffer, copy_operations_are_deleted)
{
    // Verifies secret buffers cannot be copied by construction or assignment.

    EXPECT_FALSE(std::is_copy_constructible<tpmkit::secret_buffer>::value);
    EXPECT_FALSE(std::is_copy_assignable<tpmkit::secret_buffer>::value);
    EXPECT_TRUE(std::is_move_constructible<tpmkit::secret_buffer>::value);
    EXPECT_TRUE(std::is_move_assignable<tpmkit::secret_buffer>::value);
}

TEST(secret_buffer, destructor_zeroes_memory_before_release)
{
    // Verifies destruction cleanses the underlying bytes before storage release.

    const std::vector<std::uint8_t> secret{0x11U, 0x22U, 0x33U, 0x44U};
    observer_state state;
    lifecycle_observer_scope observer{state};

    {
        const tpmkit::secret_buffer buffer{secret};
        ASSERT_EQ(buffer.size(), secret.size());
    }

    EXPECT_EQ(state.before_cleanse, secret);
    ASSERT_EQ(state.after_cleanse.size(), secret.size());
    EXPECT_TRUE(std::all_of(state.after_cleanse.begin(), state.after_cleanse.end(),
                            [](const std::uint8_t value) { return value == 0U; }));
}

TEST(secret_buffer, attempts_memory_locking_and_unlocks_before_cleanse)
{
    // Verifies construction attempts page locking and destruction unlocks before cleansing.

    const std::vector<std::uint8_t> secret{0x71U, 0x72U, 0x73U, 0x74U};
    observer_state state;
    lifecycle_observer_scope observer{state};

    {
        const tpmkit::secret_buffer buffer{secret};
        EXPECT_EQ(buffer.size(), secret.size());
    }

    EXPECT_TRUE(state.lock_attempted);
    EXPECT_EQ(state.lock_size, secret.size());
    if (memory_locking_should_succeed(secret.size())) {
        EXPECT_TRUE(state.locked);
        EXPECT_TRUE(state.unlocked);
    }
    EXPECT_FALSE(state.before_cleanse.empty());
}

} // namespace
