#include <tpmkit/testing/fake_tcti.h>

#include <tss2/tss2_common.h>
#include <tss2/tss2_tcti.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <type_traits>
#include <variant>
#include <vector>

namespace tpmkit::testing {

namespace {

constexpr std::uint64_t fake_tcti_magic = 0x46414b4554435449ULL;
constexpr std::uint32_t tcti_abi_version_v1 = 1U;

struct response_bytes {
    std::vector<std::uint8_t> bytes;
};

struct response_factory {
    std::function<std::vector<std::uint8_t>(const std::vector<std::uint8_t>&)> factory;
};

struct failure_rc {
    TSS2_RC rc;
};

struct transmit_failure_rc {
    TSS2_RC rc;
};

using queued_result =
    std::variant<response_bytes, response_factory, failure_rc, transmit_failure_rc>;

struct fake_tcti_storage {
    TSS2_TCTI_CONTEXT_COMMON_V1 common;
    void* owner;
};

static_assert(offsetof(fake_tcti_storage, common) == 0U,
              "fake_tcti_storage must begin with TSS2_TCTI_CONTEXT_COMMON_V1");
static_assert(offsetof(TSS2_TCTI_CONTEXT_COMMON_V1, magic) == 0U,
              "TSS2_TCTI_CONTEXT_COMMON_V1 magic must remain the first ABI field");
static_assert(
    std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::transmit), TSS2_TCTI_TRANSMIT_FCN>::value,
    "TCTI transmit function pointer type changed");
static_assert(
    std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::receive), TSS2_TCTI_RECEIVE_FCN>::value,
    "TCTI receive function pointer type changed");
static_assert(
    std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::finalize), TSS2_TCTI_FINALIZE_FCN>::value,
    "TCTI finalize function pointer type changed");
static_assert(
    std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::cancel), TSS2_TCTI_CANCEL_FCN>::value,
    "TCTI cancel function pointer type changed");
static_assert(std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::getPollHandles),
                           TSS2_TCTI_GET_POLL_HANDLES_FCN>::value,
              "TCTI poll-handle function pointer type changed");
static_assert(std::is_same<decltype(TSS2_TCTI_CONTEXT_COMMON_V1::setLocality),
                           TSS2_TCTI_SET_LOCALITY_FCN>::value,
              "TCTI locality function pointer type changed");

TSS2_TCTI_CONTEXT* as_context(fake_tcti_storage& storage) noexcept
{
    return reinterpret_cast<TSS2_TCTI_CONTEXT*>(&storage);
}

fake_tcti_storage* as_storage(TSS2_TCTI_CONTEXT* const context) noexcept
{
    return reinterpret_cast<fake_tcti_storage*>(context);
}

TSS2_RC copy_response(const response_bytes& response, std::size_t* const size,
                      std::uint8_t* const destination)
{
    if (size == nullptr) {
        return TSS2_TCTI_RC_BAD_REFERENCE;
    }

    const std::size_t required_size = response.bytes.size();
    if (destination == nullptr) {
        *size = required_size;
        return TSS2_RC_SUCCESS;
    }

    if (*size < required_size) {
        *size = required_size;
        return TSS2_TCTI_RC_INSUFFICIENT_BUFFER;
    }

    std::copy(response.bytes.begin(), response.bytes.end(), destination);
    *size = required_size;
    return TSS2_RC_SUCCESS;
}

} // namespace

class fake_tcti::impl final {
public:
    impl()
        : storage_{
              TSS2_TCTI_CONTEXT_COMMON_V1{
                  fake_tcti_magic,
                  tcti_abi_version_v1,
                  &impl::transmit,
                  &impl::receive,
                  &impl::finalize,
                  &impl::cancel,
                  &impl::get_poll_handles,
                  &impl::set_locality,
              },
              this,
          }
    {}

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    TSS2_TCTI_CONTEXT* handle() noexcept
    {
        return as_context(storage_);
    }

    [[nodiscard]] std::size_t pending_responses() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mu_};
        return queue_.size();
    }

    [[nodiscard]] std::vector<std::vector<std::uint8_t>> transmitted_commands() const
    {
        const std::lock_guard<std::mutex> lock{mu_};
        return transmitted_commands_;
    }

    void push_failure(const std::uint32_t tss_rc)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        queue_.push_back(failure_rc{static_cast<TSS2_RC>(tss_rc)});
    }

    void push_transmit_failure(const std::uint32_t tss_rc)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        queue_.push_back(transmit_failure_rc{static_cast<TSS2_RC>(tss_rc)});
    }

    void push_response(std::vector<std::uint8_t> bytes)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        queue_.push_back(response_bytes{std::move(bytes)});
    }

    void push_response_factory(
        std::function<std::vector<std::uint8_t>(const std::vector<std::uint8_t>&)> factory)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        queue_.push_back(response_factory{std::move(factory)});
    }

    [[nodiscard]] std::size_t transmits_observed() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mu_};
        return transmits_observed_;
    }

    [[nodiscard]] std::size_t finalizes_observed() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mu_};
        return finalizes_observed_;
    }

private:
    static TSS2_RC cancel(TSS2_TCTI_CONTEXT* const context) noexcept
    {
        if (context == nullptr) {
            return TSS2_TCTI_RC_BAD_REFERENCE;
        }

        return TSS2_RC_SUCCESS;
    }

    static void finalize(TSS2_TCTI_CONTEXT* const context) noexcept
    {
        if (context == nullptr) {
            return;
        }

        fake_tcti_storage* const storage = as_storage(context);
        if (storage->owner != nullptr) {
            static_cast<impl*>(storage->owner)->observe_finalize();
        }
        storage->owner = nullptr;
    }

    static TSS2_RC get_poll_handles(TSS2_TCTI_CONTEXT* const context, TSS2_TCTI_POLL_HANDLE*,
                                    std::size_t* const num_handles) noexcept
    {
        if (context == nullptr || num_handles == nullptr) {
            return TSS2_TCTI_RC_BAD_REFERENCE;
        }

        *num_handles = 0U;
        return TSS2_RC_SUCCESS;
    }

    static TSS2_RC receive(TSS2_TCTI_CONTEXT* const context, std::size_t* const size,
                           std::uint8_t* const response, int32_t) noexcept
    {
        try {
            impl* const self = owner_from(context);
            if (self == nullptr) {
                return TSS2_TCTI_RC_BAD_REFERENCE;
            }

            return self->receive_next(size, response);
        } catch (...) {
            return TSS2_TCTI_RC_GENERAL_FAILURE;
        }
    }

    TSS2_RC receive_next(std::size_t* const size, std::uint8_t* const response)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        if (queue_.empty()) {
            return TSS2_TCTI_RC_IO_ERROR;
        }

        queued_result& next = queue_.front();

        if (const auto* const failure = std::get_if<failure_rc>(&next)) {
            if (size != nullptr) {
                *size = 0U;
            }
            const TSS2_RC rc = failure->rc;
            queue_.pop_front();
            return rc;
        }

        if (std::get_if<transmit_failure_rc>(&next) != nullptr) {
            return TSS2_TCTI_RC_BAD_SEQUENCE;
        }

        response_bytes materialized{};
        if (const auto* const scripted = std::get_if<response_bytes>(&next)) {
            materialized = *scripted;
        } else {
            const auto& dynamic = std::get<response_factory>(next);
            const std::vector<std::uint8_t> empty_command;
            const std::vector<std::uint8_t>& last_command =
                transmitted_commands_.empty() ? empty_command : transmitted_commands_.back();
            materialized.bytes = dynamic.factory(last_command);
        }

        const TSS2_RC rc = copy_response(materialized, size, response);
        if (rc == TSS2_RC_SUCCESS && response != nullptr) {
            queue_.pop_front();
            return rc;
        }

        return rc;
    }

    static impl* owner_from(TSS2_TCTI_CONTEXT* const context) noexcept
    {
        if (context == nullptr) {
            return nullptr;
        }

        return static_cast<impl*>(as_storage(context)->owner);
    }

    static TSS2_RC set_locality(TSS2_TCTI_CONTEXT* const context, std::uint8_t) noexcept
    {
        if (context == nullptr) {
            return TSS2_TCTI_RC_BAD_REFERENCE;
        }

        return TSS2_RC_SUCCESS;
    }

    static TSS2_RC transmit(TSS2_TCTI_CONTEXT* const context, const std::size_t size,
                            const std::uint8_t* const command) noexcept
    {
        try {
            impl* const self = owner_from(context);
            if (self == nullptr || (size > 0U && command == nullptr)) {
                return TSS2_TCTI_RC_BAD_REFERENCE;
            }

            return self->observe_transmit(size, command);
        } catch (...) {
            return TSS2_TCTI_RC_GENERAL_FAILURE;
        }
    }

    TSS2_RC observe_transmit(const std::size_t size, const std::uint8_t* const command)
    {
        const std::lock_guard<std::mutex> lock{mu_};
        ++transmits_observed_;
        if (command == nullptr || size == 0U) {
            transmitted_commands_.emplace_back();
        } else {
            transmitted_commands_.emplace_back(command, command + size);
        }
        if (queue_.empty()) {
            return TSS2_TCTI_RC_IO_ERROR;
        }

        if (const auto* const failure = std::get_if<transmit_failure_rc>(&queue_.front())) {
            const TSS2_RC rc = failure->rc;
            queue_.pop_front();
            return rc;
        }

        return TSS2_RC_SUCCESS;
    }

    void observe_finalize() noexcept
    {
        const std::lock_guard<std::mutex> lock{mu_};
        ++finalizes_observed_;
    }

    std::size_t finalizes_observed_{0U};
    mutable std::mutex mu_;
    std::deque<queued_result> queue_;
    fake_tcti_storage storage_;
    std::vector<std::vector<std::uint8_t>> transmitted_commands_;
    std::size_t transmits_observed_{0U};
};

fake_tcti::fake_tcti() : impl_{std::make_unique<impl>()} {}

fake_tcti::~fake_tcti() = default;

fake_tcti::fake_tcti(fake_tcti&&) noexcept = default;

fake_tcti& fake_tcti::operator=(fake_tcti&&) noexcept = default;

TSS2_TCTI_CONTEXT* fake_tcti::handle() noexcept
{
    return impl_->handle();
}

std::size_t fake_tcti::pending_responses() const noexcept
{
    return impl_->pending_responses();
}

std::vector<std::vector<std::uint8_t>> fake_tcti::transmitted_commands() const
{
    return impl_->transmitted_commands();
}

void fake_tcti::push_failure(const std::uint32_t tss_rc)
{
    impl_->push_failure(tss_rc);
}

void fake_tcti::push_transmit_failure(const std::uint32_t tss_rc)
{
    impl_->push_transmit_failure(tss_rc);
}

void fake_tcti::push_response(std::vector<std::uint8_t> bytes)
{
    impl_->push_response(std::move(bytes));
}

void fake_tcti::push_response_factory(
    std::function<std::vector<std::uint8_t>(const std::vector<std::uint8_t>&)> factory)
{
    impl_->push_response_factory(std::move(factory));
}

std::size_t fake_tcti::transmits_observed() const noexcept
{
    return impl_->transmits_observed();
}

std::size_t fake_tcti::finalizes_observed() const noexcept
{
    return impl_->finalizes_observed();
}

} // namespace tpmkit::testing
