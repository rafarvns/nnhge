// The 19 easing curves (spec 015 FR-1, FR-2): exact ends, ranges, overshoot, closeness to Penner's equations
// computed in double with libm, and custom functions through nnh::Easing. No nnh::init() anywhere here.

#include "nnh/tween.hpp"
#include "test.hpp"

#include <cmath>
#include <limits>

using nnh::Ease;
using nnh::ease;

namespace {

constexpr double kPi = 3.14159265358979323846;

double bounce_out(double t) {
    const double n = 7.5625, d = 2.75;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) return n * (t - 1.5 / d) * (t - 1.5 / d) + 0.75;
    if (t < 2.5 / d) return n * (t - 2.25 / d) * (t - 2.25 / d) + 0.9375;
    return n * (t - 2.625 / d) * (t - 2.625 / d) + 0.984375;
}

/// Penner's equations, written from the published formulas in double precision.
double reference(Ease e, double t) {
    const double c1 = 1.70158, c2 = c1 * 1.525, c3 = c1 + 1;
    switch (e) {
    case Ease::Linear:
        return t;
    case Ease::QuadIn:
        return t * t;
    case Ease::QuadOut:
        return 1 - (1 - t) * (1 - t);
    case Ease::QuadInOut:
        return t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    case Ease::CubicIn:
        return t * t * t;
    case Ease::CubicOut:
        return 1 - std::pow(1 - t, 3);
    case Ease::CubicInOut:
        return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
    case Ease::SineIn:
        return 1 - std::cos(t * kPi / 2);
    case Ease::SineOut:
        return std::sin(t * kPi / 2);
    case Ease::SineInOut:
        return -(std::cos(kPi * t) - 1) / 2;
    case Ease::BackIn:
        return c3 * t * t * t - c1 * t * t;
    case Ease::BackOut:
        return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
    case Ease::BackInOut:
        return t < 0.5 ? (std::pow(2 * t, 2) * ((c2 + 1) * 2 * t - c2)) / 2
                       : (std::pow(2 * t - 2, 2) * ((c2 + 1) * (t * 2 - 2) + c2) + 2) / 2;
    case Ease::ElasticIn:
        return -std::pow(2, 10 * (t - 1)) * std::sin((t - 1.075) * 2 * kPi / 0.3);
    case Ease::ElasticOut:
        return std::pow(2, -10 * t) * std::sin((t - 0.075) * 2 * kPi / 0.3) + 1;
    case Ease::ElasticInOut: {
        const double u = 2 * t - 1, s = std::sin((u - 0.1125) * 2 * kPi / 0.45);
        return u < 0 ? -0.5 * std::pow(2, 10 * u) * s : std::pow(2, -10 * u) * s * 0.5 + 1;
    }
    case Ease::BounceIn:
        return 1 - bounce_out(1 - t);
    case Ease::BounceOut:
        return bounce_out(t);
    case Ease::BounceInOut:
        return t < 0.5 ? (1 - bounce_out(1 - 2 * t)) / 2 : (1 + bounce_out(2 * t - 1)) / 2;
    }
    return 0;
}

Ease curve(int i) {
    return static_cast<Ease>(i);
}

bool overshoots(Ease e) {
    return e == Ease::BackIn || e == Ease::BackOut || e == Ease::BackInOut || e == Ease::ElasticIn ||
           e == Ease::ElasticOut || e == Ease::ElasticInOut;
}

float sample(int i) {
    return static_cast<float>(i) / 10'000.0f;
}

float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

NNH_TEST(every_curve_starts_at_exactly_0_and_ends_at_exactly_1) {
    CHECK(nnh::kEaseCount == 19);
    for (int i = 0; i < nnh::kEaseCount; ++i) {
        CHECK(ease(curve(i), 0.0f) == 0.0f);
        CHECK(ease(curve(i), 1.0f) == 1.0f);
    }
}

NNH_TEST(every_curve_clamps_outside_0_1_and_returns_0_for_nan_and_tiny_t) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (int i = 0; i < nnh::kEaseCount; ++i) {
        CHECK(ease(curve(i), -0.5f) == 0.0f);
        CHECK(ease(curve(i), -inf) == 0.0f);
        CHECK(ease(curve(i), nan) == 0.0f);
        CHECK(ease(curve(i), 0x1p-25f) == 0.0f); // below 2^-24: no subnormal intermediates
        CHECK(ease(curve(i), 1.5f) == 1.0f);
        CHECK(ease(curve(i), inf) == 1.0f);
    }
}

NNH_TEST(curves_without_overshoot_stay_inside_0_1) {
    for (int i = 0; i < nnh::kEaseCount; ++i) {
        if (overshoots(curve(i))) continue;
        for (int k = 0; k <= 10'000; ++k) {
            const float v = ease(curve(i), sample(k));
            CHECK(v >= 0.0f && v <= 1.0f);
        }
    }
}

NNH_TEST(back_and_elastic_overshoot_on_the_expected_side) {
    CHECK(ease(Ease::BackIn, 0.2f) < 0.0f);
    CHECK(ease(Ease::BackOut, 0.8f) > 1.0f);
    CHECK(ease(Ease::BackInOut, 0.1f) < 0.0f && ease(Ease::BackInOut, 0.9f) > 1.0f);
    CHECK(ease(Ease::ElasticIn, 0.85f) < 0.0f);
    CHECK(ease(Ease::ElasticOut, 0.15f) > 1.0f);
    CHECK(ease(Ease::ElasticInOut, 0.4f) < 0.0f && ease(Ease::ElasticInOut, 0.6f) > 1.0f);
}

NNH_TEST(every_curve_is_within_1_4096_of_penners_equations) {
    for (int i = 0; i < nnh::kEaseCount; ++i) {
        double worst = 0;
        for (int k = 1; k < 10'000; ++k) {
            const double t = sample(k);
            worst = std::fmax(worst, std::fabs(ease(curve(i), sample(k)) - reference(curve(i), t)));
        }
        CHECK(worst < 1.0 / 4096);
    }
}

NNH_TEST(in_out_curves_pass_through_the_middle) {
    CHECK(ease(Ease::Linear, 0.5f) == 0.5f);
    CHECK(ease(Ease::QuadInOut, 0.5f) == 0.5f);
    CHECK(ease(Ease::CubicInOut, 0.5f) == 0.5f);
    CHECK(std::fabs(ease(Ease::SineInOut, 0.5f) - 0.5f) < 1e-6f);
    CHECK(std::fabs(ease(Ease::BounceInOut, 0.5f) - 0.5f) < 1e-6f);
}

NNH_TEST(easing_runs_a_builtin_curve_or_a_custom_function) {
    const nnh::Easing builtin = Ease::CubicIn;
    CHECK(builtin(0.5f) == 0.125f);
    const nnh::Easing by_default;
    CHECK(by_default(0.25f) == 0.25f);
    const nnh::Easing custom = smoothstep;
    CHECK(custom(0.5f) == 0.5f);
    CHECK(custom(0.25f) == smoothstep(0.25f));
}
