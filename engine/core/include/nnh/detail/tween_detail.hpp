#pragma once
/// @file nnh/detail/tween_detail.hpp
/// Engine detail of <nnh/tween.hpp>, in a public header only because its templates need it: the value kinds,
/// the setter adapter, the copy of a game event published on finish, and the packed start request that the
/// thin templates hand to non-template code. Not for game code.

#include "nnh/detail/finish_event.hpp"
#include "nnh/detail/small_function.hpp"
#include "nnh/detail/type_id.hpp"
#include "nnh/math.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace nnh {
struct Color; // <nnh/gfx.hpp>; only needed complete by games that animate a colour (they include gfx.hpp)
}

namespace nnh::detail {

/// What a slot animates. Timer: no value (after/every, sequence waits).
enum class TweenKind : std::uint8_t { Float, Vec2, Color, Timer, Sequence };

template <typename T>
inline constexpr bool is_tween_value =
    std::is_same_v<T, float> || std::is_same_v<T, nnh::Vec2> || std::is_same_v<T, nnh::Color>;

template <typename T>
inline constexpr TweenKind tween_kind_of = std::is_same_v<T, float>       ? TweenKind::Float
                                           : std::is_same_v<T, nnh::Vec2> ? TweenKind::Vec2
                                                                          : TweenKind::Color;

/// The type-erased setter stored in a slot: receives a pointer to the value of the slot's kind.
using SetterFn = SmallFunction<void(const void*)>;

/// Turns a `void(T)` callable into a SetterFn (same size as the callable: the 16-byte capture rule applies).
template <typename T, typename Fn> struct SetterAdapter {
    Fn fn;
    void operator()(const void* value) { fn(*static_cast<const T*>(value)); }
};

/// A setter target (spec 015 FR-5): a `void(T)` callable with at most 16 bytes of trivially copyable
/// captures, stored inline. Use it for values that may disappear before the tween ends (look the object up by
/// id and do nothing if it's gone).
template <typename T> class Setter {
  public:
    template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Setter> &&
                                                      std::is_invocable_v<std::decay_t<F>&, T>>>
    Setter(F&& fn) // NOLINT(google-explicit-constructor): `nnh::Setter<Vec2>{[this](Vec2 v) {...}}`
        : fn_(SetterAdapter<T, std::decay_t<F>>{std::forward<F>(fn)}) {}

    SetterFn fn_;
};

// FinishEvent (the copy of a game event published on finish) lives in nnh/detail/finish_event.hpp.

/// How a tween gets its end value: `to` an end, `by` a delta from the captured start, or `from_to` both
/// given.
enum class TweenMode : std::uint8_t { To, By, FromTo };

/// A start request packed by the templates of nnh::TweenSet (values are of `kind`).
struct TweenSpec {
    TweenKind kind = TweenKind::Timer;
    TweenMode mode = TweenMode::To;
    void* target = nullptr;           ///< pointer target, or null
    const SetterFn* setter = nullptr; ///< setter target, or null
    const void* from = nullptr;       ///< FromTo only
    const void* end = nullptr;        ///< the end value (To, FromTo) or the delta (By)
};

struct TweenBlock; // engine/core/src/tween/tween_block.hpp

} // namespace nnh::detail
