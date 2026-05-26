#include <tpmkit/secret_buffer.h>

#include "secret_buffer_testing.h"

#include <atomic>
#include <utility>
#include <vector>

extern "C" void OPENSSL_cleanse(void* ptr, std::size_t len);

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall VirtualLock(void* address, std::size_t size);
extern "C" __declspec(dllimport) int __stdcall VirtualUnlock(void* address, std::size_t size);
#else
extern "C" int mlock(const void* address, std::size_t size);
extern "C" int munlock(const void* address, std::size_t size);
#endif

namespace tpmkit {
namespace {

std::atomic<const detail::secret_buffer_testing::lifecycle_observer*> active_observer{nullptr};

void cleanse(std::vector<std::uint8_t>& data) noexcept
{
    if (data.empty()) {
        return;
    }

    OPENSSL_cleanse(data.data(), data.size());

    const auto* const observer = active_observer.load(std::memory_order_acquire);
    if (observer != nullptr && observer->on_cleanse != nullptr) {
        observer->on_cleanse(data.data(), data.size());
    }
}

[[nodiscard]] bool lock_memory(void* const data, const std::size_t size) noexcept
{
    if (data == nullptr || size == 0U) {
        return false;
    }

#if defined(_WIN32)
    const bool locked = VirtualLock(data, size) != 0;
#else
    const bool locked = mlock(data, size) == 0;
#endif

    const auto* const observer = active_observer.load(std::memory_order_acquire);
    if (observer != nullptr && observer->on_lock_attempt != nullptr) {
        observer->on_lock_attempt(size, locked);
    }

    return locked;
}

void unlock_memory(void* const data, const std::size_t size) noexcept
{
    if (data == nullptr || size == 0U) {
        return;
    }

#if defined(_WIN32)
    static_cast<void>(VirtualUnlock(data, size));
#else
    static_cast<void>(munlock(data, size));
#endif

    const auto* const observer = active_observer.load(std::memory_order_acquire);
    if (observer != nullptr && observer->on_unlock != nullptr) {
        observer->on_unlock(size);
    }
}

} // namespace

namespace detail::secret_buffer_testing {

void set_lifecycle_observer(const lifecycle_observer* const observer) noexcept
{
    active_observer.store(observer, std::memory_order_release);
}

} // namespace detail::secret_buffer_testing

class secret_buffer::impl final {
public:
    explicit impl(std::vector<std::uint8_t> data) : data_(std::move(data))
    {
        locked_ = lock_memory(data_.data(), data_.size());
    }

    ~impl() noexcept
    {
        clear();
    }

    impl(const impl& other) = delete;
    impl& operator=(const impl& other) = delete;
    impl(impl&& other) noexcept = delete;
    impl& operator=(impl&& other) noexcept = delete;

    void clear() noexcept
    {
        if (data_.empty()) {
            return;
        }

        if (locked_) {
            unlock_memory(data_.data(), data_.size());
            locked_ = false;
        }

        cleanse(data_);
        data_.clear();
    }

    [[nodiscard]] const std::vector<std::uint8_t>& data() const noexcept
    {
        return data_;
    }

private:
    std::vector<std::uint8_t> data_;
    bool locked_ = false;
};

secret_buffer::secret_buffer() noexcept = default;

secret_buffer::secret_buffer(std::vector<std::uint8_t>&& data)
    : impl_(std::make_unique<impl>(std::move(data)))
{}

secret_buffer::~secret_buffer() noexcept = default;

secret_buffer::secret_buffer(secret_buffer&& other) noexcept = default;

secret_buffer& secret_buffer::operator=(secret_buffer&& other) noexcept = default;

bool secret_buffer::empty() const noexcept
{
    return impl_ == nullptr || impl_->data().empty();
}

std::size_t secret_buffer::size() const noexcept
{
    if (impl_ == nullptr) {
        return 0U;
    }

    return impl_->data().size();
}

gsl::span<const std::uint8_t> secret_buffer::view() const noexcept
{
    if (impl_ == nullptr || impl_->data().empty()) {
        return {};
    }

    return gsl::span<const std::uint8_t>(impl_->data().data(), impl_->data().size());
}

} // namespace tpmkit
