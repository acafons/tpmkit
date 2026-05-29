# Code skeletons

Illustrative skeletons for the shapes discussed in `SKILL.md`. They use
plausible names — `digest_sha256`, `tpm_session`, `key::provider` — to show the
*shape*. Real types in the repo may differ; copy the pattern, not the names.
Each example is paired with a short note on what is load-bearing about it.

## 1. Value object — `digest_sha256`

```cpp
#include <array>
#include <cstdint>

class digest_sha256 final {
public:
    static constexpr std::size_t size_bytes = 32;
    using bytes_type = std::array<std::uint8_t, size_bytes>;

    explicit digest_sha256(bytes_type bytes) noexcept : bytes_(bytes) {}

    [[nodiscard]] const bytes_type& bytes() const noexcept { return bytes_; }

    friend bool operator==(const digest_sha256& a, const digest_sha256& b) noexcept {
        return a.bytes_ == b.bytes_;
    }
    friend bool operator!=(const digest_sha256& a, const digest_sha256& b) noexcept {
        return !(a == b);
    }

private:
    bytes_type bytes_;
};
```

The 32-byte invariant lives in the type (`std::array<…, 32>`), not in a runtime check. Constructor is `explicit`. No setters. Equality compares content. No identity. A digest is a hash, not a secret — plain `==` is appropriate; secret-derived comparisons use `CRYPTO_memcmp` (`security.md`).

## 2. Entity — `tpm_session`

```cpp
class tpm_session {
public:
    explicit tpm_session(session_handle handle) noexcept : handle_(handle) {}

    [[nodiscard]] session_handle handle() const noexcept { return handle_; }
    [[nodiscard]] const secret_buffer& nonce() const noexcept { return nonce_; }

    void rotate_nonce(secret_buffer next) noexcept { nonce_ = std::move(next); }

    friend bool operator==(const tpm_session& a, const tpm_session& b) noexcept {
        return a.handle_ == b.handle_;  // identity, not state
    }

private:
    session_handle handle_;
    secret_buffer nonce_;
};
```

Identity (`handle_`) is explicit and immutable. State (`nonce_`) changes over the session's lifetime. Two sessions with the same handle are the same entity even if their nonces differ — equality reflects that. The nonce is a `secret_buffer` (`security.md`), never a raw `std::vector<uint8_t>`.

## 3. Domain service — free function `sign`

```cpp
// In src/domain/signing_service.h — pure domain. No third-party headers.
[[nodiscard]] outcome<signature, error>
sign(key::provider& provider,
     const key_id& id,
     const message_bytes& message);
```

Free function, no state. Takes a port by reference plus value objects, returns an `outcome` (`error-handling.md`). The operation spans a key (entity) and a message (value object); it doesn't naturally belong on either, so it lives on its own. Distinguish from a *port* — `key::provider` describes what the domain needs from outside; `sign` is logic inside the domain that uses the port.

## 4. RAII wrapper — `tss2_context`

```cpp
// In src/adapters/tpm2_esys/context/tss2_context.h
class tss2_context final {
public:
    tss2_context();
    ~tss2_context();

    tss2_context(const tss2_context&)            = delete;
    tss2_context& operator=(const tss2_context&) = delete;
    tss2_context(tss2_context&& other) noexcept;
    tss2_context& operator=(tss2_context&& other) noexcept;

    [[nodiscard]] ESYS_CONTEXT* native() const noexcept { return ctx_; }

private:
    ESYS_CONTEXT* ctx_{nullptr};
};
```

```cpp
// In src/adapters/tpm2_esys/context/tss2_context.cpp
tss2_context::tss2_context() {
    if (Esys_Initialize(&ctx_, nullptr, nullptr) != TSS2_RC_SUCCESS) {
        throw resource_error{"failed to initialize ESYS context"};
    }
}
tss2_context::~tss2_context() {
    if (ctx_ != nullptr) Esys_Finalize(&ctx_);
}
tss2_context::tss2_context(tss2_context&& other) noexcept : ctx_(other.ctx_) {
    other.ctx_ = nullptr;
}
// move-assignment: release current, take other.ctx_, null out other.ctx_.
```

Lives in the adapter folder; the domain never includes `<tss2/...>`. Constructor either initializes the resource or throws — no two-phase init. Destructor is the only place `Esys_Finalize` is called; use sites never do manual cleanup. Copy deleted because TSS contexts cannot be duplicated; move transfers ownership and leaves the source null.

## 5. Port + adapter — `key::provider`

```cpp
// In include/tpmkit/key/provider.h — public domain port. Domain types only.
namespace tpmkit::key {

class provider {
public:
    virtual ~provider() = default;

    [[nodiscard]] virtual outcome<key_id, error>
    create_signing_key(algorithm_id algo) = 0;

    [[nodiscard]] virtual outcome<signature, error>
    sign(const key_id& id, const message_bytes& message) = 0;
};

} // namespace tpmkit::key
```

```cpp
// Public test-helper header in include/tpmkit/testing/mock_key_provider.h.
// Implementation in src/adapters/mock/mock_key_provider.cpp.
class mock_key_provider final : public key::provider {
public:
    outcome<key_id, error>   create_signing_key(algorithm_id) override;
    outcome<signature, error> sign(const key_id&, const message_bytes&) override;

    // Adapter-specific seam: pre-load a deterministic response.
    void seed(key_id, signature canned);

private:
    std::unordered_map<key_id, signature> canned_;
};
```

Port signatures use only domain types — no `TSS2_*` or `EVP_*`. Real adapters (`tpm2_fapi_adapter`, `tpm2_esys_adapter`, `software_key_provider`) live alongside the mock and translate third-party errors into the domain `error` at their boundary. Production code holds the port type, never the concrete adapter.

## 6. Visitor over `std::variant` — `policy_node`

```cpp
#include <variant>

using policy_node = std::variant<policy_or, policy_and, policy_pcr, policy_signed>;

namespace detail {
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
}

[[nodiscard]] bool evaluate(const policy_node& node, const policy_context& ctx) {
    return std::visit(detail::overloaded{
        [&](const policy_or& n)     { return evaluate_or(n, ctx); },
        [&](const policy_and& n)    { return evaluate_and(n, ctx); },
        [&](const policy_pcr& n)    { return evaluate_pcr(n, ctx); },
        [&](const policy_signed& n) { return evaluate_signed(n, ctx); },
    }, node);
}
```

Exhaustive at compile time: adding a new alternative to `policy_node` breaks the build until `evaluate` grows a matching lambda. That pressure is the value — preferable to `dynamic_cast` chains or `if`/`else if` on a type tag, which fail silently when a case is forgotten. The `overloaded` helper lives under `detail::`, not in the public namespace.

## 7. Null object — `null_logger`

```cpp
// In src/adapters/null_logger.h
class null_logger final : public logger {
public:
    void log(level, std::string_view, std::initializer_list<field>) noexcept override {}
};
```

Satisfies the `logger` port without doing the work. Wired by the composition root when no real logger is configured. Because `null_logger` is always present, domain code never branches on `if (logger)` — it just calls `log(...)`. Same pattern recurs for absent-but-required dependencies: implement the port with a no-op rather than scatter null checks.
