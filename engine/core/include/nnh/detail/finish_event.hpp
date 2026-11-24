#pragma once
/// @file nnh/detail/finish_event.hpp
/// A copy of one of the game's events, queued on the bus when something finishes (a tween's or a transition's
/// `.publish`). Engine detail in a public header because the options structs hold it. Not for game code.

#include "nnh/detail/type_id.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace nnh::detail {

/// A copy of one of the game's events, published on the bus when a tween, timer or sequence finishes
/// (spec 015 FR-11): `.publish = IntroFinished{}`. Trivially copyable, at most 32 bytes, aligned to at
/// most 8.
struct FinishEvent {
    static constexpr std::size_t kMaxSize = 32;

    FinishEvent() = default;
    // Not for callables: `after(t, [this] {...})` must pick the callback overload.
    template <typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, FinishEvent> &&
                                                      !std::is_invocable_v<const T&>>>
    FinishEvent(const T& event) // NOLINT(google-explicit-constructor): `.publish = IntroFinished{}`
        : type(type_id<T>()), size(static_cast<std::uint32_t>(sizeof(T))) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "nnh: events published on finish must be trivially copyable");
        static_assert(
            sizeof(T) <= kMaxSize,
            "nnh: event published on finish too large (max 32 bytes): send an id instead of the data");
        static_assert(alignof(T) <= 8, "nnh: event published on finish is over-aligned (max 8)");
        std::memcpy(bytes, &event, sizeof(T));
    }

    explicit operator bool() const { return type != nullptr; }

    TypeId type = nullptr;
    std::uint32_t size = 0;
    alignas(8) unsigned char bytes[kMaxSize] = {};
};

} // namespace nnh::detail
