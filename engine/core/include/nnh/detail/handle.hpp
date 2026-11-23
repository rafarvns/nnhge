#pragma once
/// @file nnh/detail/handle.hpp
/// Generational handles (spec 020 FR-1, decision D35), shared by <nnh/pool.hpp> and <nnh/entities.hpp>.

#include <cstdint>

namespace nnh {

/// A 32-bit generational handle: a slot (bits 0–15) and a generation (bits 16–31); all zero is the null handle.
/// Trivially copyable, 4 bytes: store it in your structs and events. A handle says nothing about liveness by itself:
/// ask its pool (`pool.alive(h)`) or world (`world.alive(e)`). Every release or destroy of a slot raises its
/// generation, so every handle issued before reads as stale from that moment (until the generation wraps: 65 535
/// reuses of one slot, see docs/api/pool.md).
/// @code
/// nnh::Handle<Bullet> h = bullets.acquire().handle;
/// if (Bullet* b = bullets.get(h)) b->x += 1; // null once released
/// @endcode
template <typename T>
class Handle {
public:
    constexpr Handle() = default;

    constexpr int slot() const { return static_cast<int>(bits_ & 0xFFFFu); }
    constexpr int generation() const { return static_cast<int>(bits_ >> 16); }
    /// The handle as one word (for checksums, logs and your own packing).
    constexpr std::uint32_t bits() const { return bits_; }
    static constexpr Handle from_bits(std::uint32_t bits) {
        Handle h;
        h.bits_ = bits;
        return h;
    }
    /// Not null (it may still be stale).
    constexpr explicit operator bool() const { return bits_ != 0; }
    constexpr bool operator==(const Handle&) const = default;

private:
    std::uint32_t bits_ = 0;
};

namespace detail {

/// The handle of `slot` at `generation` (generation ≥ 1 for a live object).
template <typename T>
constexpr Handle<T> make_handle(std::uint32_t slot, std::uint32_t generation) {
    return Handle<T>::from_bits(slot | (generation << 16));
}

} // namespace detail
} // namespace nnh
