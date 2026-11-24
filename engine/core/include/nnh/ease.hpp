#pragma once
/// @file nnh/ease.hpp
/// Easing curves (spec 015 FR-1..FR-3): the 19 standard curves, evaluated without libm so they give the same bits
/// on every compiler and system, and the Easing value accepted wherever a curve is (tweens, transitions). Light: only
/// <cstdint>. <nnh/tween.hpp> includes it. See docs/api/tween.md.

#include <cstdint>

namespace nnh {

/// The standard easing curves (Robert Penner's equations): back overshoots by 1.70158, elastic uses period 0.3
/// (0.45 for in-out), bounce has four bounces.
enum class Ease : std::uint8_t {
    Linear,
    QuadIn, QuadOut, QuadInOut,
    CubicIn, CubicOut, CubicInOut,
    SineIn, SineOut, SineInOut,
    BackIn, BackOut, BackInOut,
    ElasticIn, ElasticOut, ElasticInOut,
    BounceIn, BounceOut, BounceInOut,
};
inline constexpr int kEaseCount = 19;

/// A game's own curve: progress in [0, 1] → eased value.
using EaseFn = float (*)(float t);

/// Evaluates `curve` at progress `t`. Exactly 0 for t ≤ 0, NaN and 0 < t < 2⁻²⁴; exactly 1 for t ≥ 1. Back and
/// elastic leave [0, 1] in between; the others stay inside it. Same bits on every compiler and system (no libm:
/// own approximations, within 2⁻¹² of the exact curve; see docs/api/tween.md). Pure: no state, no init needed,
/// allocates nothing.
/// @code
/// const float x = 10 + 200 * nnh::ease(nnh::Ease::CubicOut, t);
/// @endcode
float ease(Ease curve, float t);

/// A built-in curve or the game's own function, accepted wherever a curve is: `.ease = nnh::Ease::BackOut` or
/// `.ease = my_curve`. Custom functions are outside the determinism guarantee (the game's own float code).
struct Easing {
    // NOLINTNEXTLINE(google-explicit-constructor): implicit on purpose.
    constexpr Easing(Ease e = Ease::Linear) : builtin(e) {}
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr Easing(EaseFn f) : custom(f) {}

    /// custom ? custom(t) : ease(builtin, t).
    float operator()(float t) const;

    Ease builtin = Ease::Linear;
    EaseFn custom = nullptr;
};

} // namespace nnh
