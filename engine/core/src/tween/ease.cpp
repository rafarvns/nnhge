// The 19 easing curves (spec 015 FR-1..FR-3), re-derived from Robert Penner's published equations (BSD).
// Only IEEE float + − × ÷ and the approximations of ease_math.cpp: the same bits on every system.

#include "nnh/tween.hpp"
#include "tween/ease_math.hpp"

namespace nnh {
namespace {

constexpr float kBack = 1.70158f;            // back overshoot
constexpr float kBackInOut = kBack * 1.525f; // back in-out overshoot

float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float quad_in_out(float t) {
    if (t < 0.5f) return 2.0f * t * t;
    const float u = 2.0f - 2.0f * t;
    return 1.0f - u * u * 0.5f;
}

float cubic_out(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float cubic_in_out(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    const float u = 2.0f - 2.0f * t;
    return 1.0f - u * u * u * 0.5f;
}

/// cos(π·t) for t in [0, 1].
float cos_half_turn(float t) {
    return t <= 0.5f ? detail::sin_quarter(1.0f - 2.0f * t) : -detail::sin_quarter(2.0f * t - 1.0f);
}

float back_in(float t) {
    return (kBack + 1.0f) * t * t * t - kBack * t * t;
}

float back_out(float t) {
    const float u = t - 1.0f;
    return 1.0f + (kBack + 1.0f) * u * u * u + kBack * u * u;
}

float back_in_out(float t) {
    const float u = 2.0f * t;
    if (t < 0.5f) return u * u * ((kBackInOut + 1.0f) * u - kBackInOut) * 0.5f;
    const float w = u - 2.0f;
    return (w * w * ((kBackInOut + 1.0f) * w + kBackInOut) + 2.0f) * 0.5f;
}

float elastic_in(float t) {
    const float u = t - 1.0f;
    return -detail::exp2_neg(10.0f * u) * detail::sin_turns((u - 0.075f) / 0.3f);
}

float elastic_out(float t) {
    return detail::exp2_neg(-10.0f * t) * detail::sin_turns((t - 0.075f) / 0.3f) + 1.0f;
}

float elastic_in_out(float t) {
    const float u = 2.0f * t - 1.0f;
    const float wave = detail::sin_turns((u - 0.1125f) / 0.45f);
    if (u < 0.0f) return -0.5f * detail::exp2_neg(10.0f * u) * wave;
    return detail::exp2_neg(-10.0f * u) * wave * 0.5f + 1.0f;
}

float bounce_out(float t) {
    constexpr float n = 7.5625f, d = 2.75f;
    float v;
    if (t < 1.0f / d) {
        v = n * t * t;
    } else if (t < 2.0f / d) {
        const float u = t - 1.5f / d;
        v = n * u * u + 0.75f;
    } else if (t < 2.5f / d) {
        const float u = t - 2.25f / d;
        v = n * u * u + 0.9375f;
    } else {
        const float u = t - 2.625f / d;
        v = n * u * u + 0.984375f;
    }
    return v < 1.0f ? v : 1.0f;
}

float bounce_in_out(float t) {
    if (t < 0.5f) return (1.0f - bounce_out(1.0f - 2.0f * t)) * 0.5f;
    return (1.0f + bounce_out(2.0f * t - 1.0f)) * 0.5f;
}

float curve_at(Ease curve, float t) {
    switch (curve) {
    case Ease::Linear:
        return t;
    case Ease::QuadIn:
        return t * t;
    case Ease::QuadOut:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case Ease::QuadInOut:
        return quad_in_out(t);
    case Ease::CubicIn:
        return t * t * t;
    case Ease::CubicOut:
        return cubic_out(t);
    case Ease::CubicInOut:
        return cubic_in_out(t);
    case Ease::SineIn:
        return 1.0f - detail::sin_quarter(1.0f - t);
    case Ease::SineOut:
        return detail::sin_quarter(t);
    case Ease::SineInOut:
        return clamp01((1.0f - cos_half_turn(t)) * 0.5f);
    case Ease::BackIn:
        return back_in(t);
    case Ease::BackOut:
        return back_out(t);
    case Ease::BackInOut:
        return back_in_out(t);
    case Ease::ElasticIn:
        return elastic_in(t);
    case Ease::ElasticOut:
        return elastic_out(t);
    case Ease::ElasticInOut:
        return elastic_in_out(t);
    case Ease::BounceIn:
        return 1.0f - bounce_out(1.0f - t);
    case Ease::BounceOut:
        return bounce_out(t);
    case Ease::BounceInOut:
        return bounce_in_out(t);
    }
    return t; // not an Ease value: linear
}

} // namespace

float ease(Ease curve, float t) {
    if (!(t >= 0x1p-24f)) return 0.0f; // t ≤ 0, NaN and tiny t: no subnormal intermediate ever (t³ ≥ 2⁻⁷²)
    if (t >= 1.0f) return 1.0f;
    return curve_at(curve, t);
}

float Easing::operator()(float t) const {
    return custom != nullptr ? custom(t) : ease(builtin, t);
}

} // namespace nnh
