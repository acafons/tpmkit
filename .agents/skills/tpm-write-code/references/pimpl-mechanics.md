# Pimpl mechanics

The decision to use Pimpl is in `.claude/rules/library-api-design.md` ("Pimpl idiom"). This file covers the C++17 mechanics — the small details that, if missed, produce subtle bugs (incomplete-type errors, default destructor in the wrong place, accidental copy semantics).

## The basic shape

In the public header (`include/<library>/foo.h`):

```cpp
#pragma once

#include <memory>

namespace libname {

class foo final {
public:
    foo();
    ~foo();                 // declared here, defined in the .cpp
    foo(foo&&) noexcept;
    foo& operator=(foo&&) noexcept;

    // public methods …

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace libname
```

In the implementation file (`src/foo.cpp`):

```cpp
#include "libname/foo.h"

#include <openssl/...>      // adapter dependencies live here, not in the header

namespace libname {

struct foo::impl {
    // private state — anything that would otherwise leak via the header
};

foo::foo() : impl_(std::make_unique<impl>()) {}
foo::~foo() = default;
foo::foo(foo&&) noexcept = default;
foo& foo::operator=(foo&&) noexcept = default;

} // namespace libname
```

## Why the destructor must be defined in the `.cpp`

`std::unique_ptr<impl>` instantiates `impl`'s destructor at the point where the `unique_ptr`'s destructor is instantiated. If the compiler synthesises `foo`'s destructor in a translation unit that has only seen the forward declaration of `impl`, it fails with an "incomplete type" error.

Declaring the destructor in the header and defining it (even as `= default`) in the `.cpp` — where `impl` is complete — fixes this. The same reasoning applies to move operations: declare in header, define in `.cpp`.

## Move-only by default

Pimpl classes are move-only because the default copy constructor would shallow-copy the `unique_ptr`, which is not copyable. Don't implement copy unless the class genuinely has value semantics.

If copy *is* required:

```cpp
foo::foo(const foo& other) : impl_(std::make_unique<impl>(*other.impl_)) {}
foo& foo::operator=(const foo& other) {
    if (this != &other) {
        impl_ = std::make_unique<impl>(*other.impl_);
    }
    return *this;
}
```

`impl` itself must be copyable (or you must write the copy of `impl` by hand). Verify the deep-copy semantics match what callers expect — a half-deep copy is worse than no copy at all.

## Common mistakes

- **Default-defining the destructor in the header.** Compiles when `impl` is incomplete, fails at the use site. Always move the definition to the `.cpp`.
- **Forgetting `noexcept` on move operations.** `library-api-design.md` requires public move operations to be `noexcept`; the synthesised defaults usually are, but only if `impl`'s moves are also `noexcept`. Check.
- **Applying Pimpl to small value types.** A 16-byte digest does not need a heap allocation. Pimpl pays off when internals are non-trivial *and* likely to evolve; small immutable values fail both tests.
- **Exposing `impl` through a public method.** Defeats the whole purpose. If a friend or accessor is needed, redesign — it usually means `impl` has grown a responsibility that belongs on `foo` itself.

## When you are migrating an existing class to Pimpl

1. Identify the private state that is leaking through the header (third-party types, unstable internals).
2. Move that state into a new `impl` struct, defined in the `.cpp`.
3. Replace each direct member access in the methods with `impl_->member`.
4. Declare the destructor and move operations in the header; define them in the `.cpp` with `= default`.
5. Decide on copy semantics deliberately — most migrations end up move-only.
6. Re-run the test suite under ASan; lifetime bugs from the shuffle are common and ASan catches them.
