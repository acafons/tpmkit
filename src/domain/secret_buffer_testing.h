#pragma once

#include <cstddef>
#include <cstdint>

namespace tpmkit::detail::secret_buffer_testing {

struct lifecycle_observer {
    void (*on_cleanse)(const std::uint8_t* after, std::size_t size) noexcept = nullptr;
    void (*on_lock_attempt)(std::size_t size, bool locked) noexcept = nullptr;
    void (*on_unlock)(std::size_t size) noexcept = nullptr;
};

void set_lifecycle_observer(const lifecycle_observer* observer) noexcept;

} // namespace tpmkit::detail::secret_buffer_testing
