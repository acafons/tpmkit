#include <tpmkit/testing/mock_pcr_provider.h>

#include <tl/expected.hpp>

#include <utility>

namespace tpmkit::testing {

namespace {

[[nodiscard]] error make_default_error()
{
    return error{error_category::backend_error, "mock PCR provider response was not programmed"};
}

template <class T> [[nodiscard]] outcome<T> make_default_failure()
{
    return tl::unexpected(make_default_error());
}

[[nodiscard]] outcome<void> make_default_void_failure()
{
    return tl::unexpected(make_default_error());
}

} // namespace

mock_pcr_provider::mock_pcr_provider()
    : allocate_result_{make_default_failure<pcr::allocate_result>()},
      event_result_{make_default_failure<pcr::event_result>()},
      extend_result_{make_default_void_failure()},
      read_result_{make_default_failure<pcr::read_result>()},
      reset_result_{make_default_void_failure()},
      set_auth_policy_result_{make_default_void_failure()},
      set_auth_value_result_{make_default_void_failure()}
{}

outcome<pcr::allocate_result> mock_pcr_provider::allocate(gsl::span<const pcr::bank>)
{
    ++allocate_call_count_;
    return allocate_result_;
}

std::size_t mock_pcr_provider::allocate_call_count() const noexcept
{
    return allocate_call_count_;
}

void mock_pcr_provider::clear_call_counts() noexcept
{
    allocate_call_count_ = 0U;
    event_call_count_ = 0U;
    extend_call_count_ = 0U;
    read_call_count_ = 0U;
    reset_call_count_ = 0U;
    set_auth_policy_call_count_ = 0U;
    set_auth_value_call_count_ = 0U;
}

outcome<pcr::event_result> mock_pcr_provider::event(pcr::index, gsl::span<const std::uint8_t>)
{
    ++event_call_count_;
    return event_result_;
}

std::size_t mock_pcr_provider::event_call_count() const noexcept
{
    return event_call_count_;
}

outcome<void> mock_pcr_provider::extend(pcr::index, gsl::span<const pcr::digest_value>)
{
    ++extend_call_count_;
    return extend_result_;
}

std::size_t mock_pcr_provider::extend_call_count() const noexcept
{
    return extend_call_count_;
}

outcome<pcr::read_result> mock_pcr_provider::read(const pcr::selection&)
{
    ++read_call_count_;
    return read_result_;
}

std::size_t mock_pcr_provider::read_call_count() const noexcept
{
    return read_call_count_;
}

outcome<void> mock_pcr_provider::reset(pcr::index)
{
    ++reset_call_count_;
    return reset_result_;
}

std::size_t mock_pcr_provider::reset_call_count() const noexcept
{
    return reset_call_count_;
}

void mock_pcr_provider::set_allocate_result(outcome<pcr::allocate_result> result)
{
    allocate_result_ = std::move(result);
}

outcome<void> mock_pcr_provider::set_auth_policy(pcr::index, hash_algorithm,
                                                 gsl::span<const std::uint8_t>)
{
    ++set_auth_policy_call_count_;
    return set_auth_policy_result_;
}

std::size_t mock_pcr_provider::set_auth_policy_call_count() const noexcept
{
    return set_auth_policy_call_count_;
}

void mock_pcr_provider::set_auth_policy_result(outcome<void> result)
{
    set_auth_policy_result_ = std::move(result);
}

outcome<void> mock_pcr_provider::set_auth_value(pcr::index, secret_buffer)
{
    ++set_auth_value_call_count_;
    return set_auth_value_result_;
}

std::size_t mock_pcr_provider::set_auth_value_call_count() const noexcept
{
    return set_auth_value_call_count_;
}

void mock_pcr_provider::set_auth_value_result(outcome<void> result)
{
    set_auth_value_result_ = std::move(result);
}

void mock_pcr_provider::set_event_result(outcome<pcr::event_result> result)
{
    event_result_ = std::move(result);
}

void mock_pcr_provider::set_extend_result(outcome<void> result)
{
    extend_result_ = std::move(result);
}

void mock_pcr_provider::set_read_result(outcome<pcr::read_result> result)
{
    read_result_ = std::move(result);
}

void mock_pcr_provider::set_reset_result(outcome<void> result)
{
    reset_result_ = std::move(result);
}

} // namespace tpmkit::testing
