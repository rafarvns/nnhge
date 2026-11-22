// The libm-free approximations behind the sine and elastic curves (spec 015 FR-3, plan 015 "Determinism"):
// error bounds against <cmath> (tests may use libm), exact endpoints and the reduction across periods.

#include "test.hpp"
#include "tween/ease_math.hpp"

#include <cmath>

using nnh::detail::exp2_neg;
using nnh::detail::sin_quarter;
using nnh::detail::sin_turns;

namespace {

constexpr double kPi = 3.14159265358979323846;

double max_sin_quarter_error() {
    double worst = 0;
    for (int i = 0; i <= 100'000; ++i) {
        const float u = static_cast<float>(i) / 100'000.0f;
        worst = std::fmax(worst, std::fabs(sin_quarter(u) - std::sin(kPi / 2 * u)));
    }
    return worst;
}

double max_sin_turns_error(float from, float to) {
    double worst = 0;
    for (int i = 0; i <= 100'000; ++i) {
        const float x = from + (to - from) * static_cast<float>(i) / 100'000.0f;
        worst = std::fmax(worst, std::fabs(sin_turns(x) - std::sin(2 * kPi * x)));
    }
    return worst;
}

double max_exp2_relative_error() {
    double worst = 0;
    for (int i = 0; i <= 100'000; ++i) {
        const float x = -10.0f * static_cast<float>(i) / 100'000.0f;
        const double exact = std::exp2(static_cast<double>(x));
        worst = std::fmax(worst, std::fabs(exp2_neg(x) - exact) / exact);
    }
    return worst;
}

} // namespace

NNH_TEST(sin_quarter_is_within_2e6_of_the_exact_sine) {
    CHECK(max_sin_quarter_error() < 2e-6);
}

NNH_TEST(sin_quarter_is_exact_at_its_ends) {
    CHECK(sin_quarter(0.0f) == 0.0f);
    CHECK(sin_quarter(1.0f) == 1.0f);
}

NNH_TEST(sin_quarter_never_exceeds_one) {
    for (int i = 0; i <= 100'000; ++i) {
        CHECK(sin_quarter(static_cast<float>(i) / 100'000.0f) <= 1.0f);
    }
}

NNH_TEST(sin_turns_follows_the_sine_over_several_periods) {
    CHECK(max_sin_turns_error(0.0f, 1.0f) < 2e-6);
    // Larger |x|: the reduced fraction keeps fewer bits, so the bound is looser.
    CHECK(max_sin_turns_error(-4.0f, 0.0f) < 4e-6);
    CHECK(max_sin_turns_error(-3.6f, 3.4f) < 4e-6);
}

NNH_TEST(sin_turns_hits_the_quarter_points_exactly) {
    CHECK(sin_turns(0.0f) == 0.0f);
    CHECK(sin_turns(0.25f) == 1.0f);
    CHECK(sin_turns(0.5f) == 0.0f);
    CHECK(sin_turns(0.75f) == -1.0f);
    CHECK(sin_turns(-0.25f) == -1.0f);
    CHECK(sin_turns(2.25f) == 1.0f);
}

NNH_TEST(exp2_neg_is_within_1e6_relative_error) {
    CHECK(max_exp2_relative_error() < 1e-6);
}

NNH_TEST(exp2_neg_is_exact_at_whole_powers) {
    CHECK(exp2_neg(0.0f) == 1.0f);
    CHECK(exp2_neg(-1.0f) == 0.5f);
    CHECK(exp2_neg(-10.0f) == 1.0f / 1024.0f);
    CHECK(exp2_neg(-3.0f) == 0.125f);
}
