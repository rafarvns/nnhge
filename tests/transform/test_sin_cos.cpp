// The engine's deterministic sine and cosine (spec 022 FR-3, T1.2): exact quarter turns from the constants and their
// small multiples, odd/even symmetry on the bits, a reference table against <cmath> (tests may use libm), defined bits
// for non-finite and huge angles.

#include "../entities/world_fixture.hpp"
#include "nnh/transform.hpp"
#include "test.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

using nnh::SinCos;
using nnh::sin_cos;

namespace {

std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

constexpr double kPi = 3.14159265358979323846;

float g_angle = 0;
void sin_cos_of_g_angle() { (void)sin_cos(g_angle); }

} // namespace

NNH_TEST(sin_cos_of_zero_is_exactly_zero_and_one) {
    const SinCos sc = sin_cos(0.0f);
    CHECK(bits(sc.sin) == 0 && sc.cos == 1.0f);
}

NNH_TEST(sin_cos_constants_give_exact_quarter_turns) {
    CHECK(sin_cos(nnh::kQuarterTurn).sin == 1.0f && sin_cos(nnh::kQuarterTurn).cos == 0.0f);
    CHECK(sin_cos(nnh::kHalfTurn).sin == 0.0f && sin_cos(nnh::kHalfTurn).cos == -1.0f);
    CHECK(sin_cos(nnh::kThreeQuarterTurn).sin == -1.0f && sin_cos(nnh::kThreeQuarterTurn).cos == 0.0f);
    CHECK(sin_cos(nnh::kFullTurn).sin == 0.0f && sin_cos(nnh::kFullTurn).cos == 1.0f);
}

NNH_TEST(sin_cos_small_multiples_of_a_quarter_turn_are_exact) {
    static const float kSin[4] = {0, 1, 0, -1};
    static const float kCos[4] = {1, 0, -1, 0};
    for (int k = -8; k <= 8; ++k) {
        const SinCos sc = sin_cos(static_cast<float>(k) * nnh::kQuarterTurn);
        const int q = k & 3;
        CHECK(sc.sin == kSin[q] && sc.cos == kCos[q]);
    }
    // Half quarter turns (the platformer's 8 directions): within the polynomial's error of √½.
    for (int k = -8; k <= 8; ++k) {
        const SinCos sc = sin_cos(static_cast<float>(k) * nnh::kQuarterTurn * 0.5f);
        CHECK(std::fabs(sc.sin - std::sin(kPi / 4 * k)) < 2e-6 && std::fabs(sc.cos - std::cos(kPi / 4 * k)) < 2e-6);
    }
}

NNH_TEST(sin_cos_is_odd_and_even_on_the_bits) {
    for (int i = 1; i <= 4096; ++i) {
        const float a = static_cast<float>(i) * 0.0123f - 3.0f;
        const SinCos p = sin_cos(a), n = sin_cos(-a);
        CHECK(bits(n.sin) == (bits(p.sin) ^ 0x80000000u));
        CHECK(bits(n.cos) == bits(p.cos));
    }
}

NNH_TEST(sin_cos_matches_the_reference_within_the_polynomial_error) {
    double worst = 0;
    for (int i = 0; i < 4096; ++i) {
        const float a = (static_cast<float>(i) - 2048.0f) * (20.0f / 2048.0f); // −20..20 rad
        const SinCos sc = sin_cos(a);
        worst = std::fmax(worst, std::fabs(sc.sin - std::sin(static_cast<double>(a))));
        worst = std::fmax(worst, std::fabs(sc.cos - std::cos(static_cast<double>(a))));
    }
    CHECK(worst < 7e-7 + 2e-6); // the polynomial (< 7·10⁻⁷), the reduction's rounding and the snap
}

NNH_TEST(sin_cos_values_stay_within_one) {
    for (int i = 0; i < 100'000; ++i) {
        const SinCos sc = sin_cos(static_cast<float>(i) * 0.000731f);
        CHECK(std::fabs(sc.sin) <= 1.0f && std::fabs(sc.cos) <= 1.0f);
    }
}

NNH_TEST(sin_cos_snap_is_continuous_around_quarter_turns) {
    // Within the snap (2⁻²⁰ quarter turn ≈ 1.5·10⁻⁶ rad) the values are exact; just outside it they are the
    // polynomial's, within its error of the exact ones (no jump at the edge).
    for (int i = -64; i <= 64; ++i) {
        const float a = nnh::kQuarterTurn + static_cast<float>(i) * 1.0e-7f;
        const SinCos sc = sin_cos(a);
        const bool snapped = std::fabs(static_cast<double>(a) - kPi / 2) < 1.4e-6;
        if (snapped) CHECK(sc.sin == 1.0f && sc.cos == 0.0f);
        CHECK(std::fabs(sc.sin - std::sin(static_cast<double>(a))) < 2e-6);
        CHECK(std::fabs(sc.cos - std::cos(static_cast<double>(a))) < 2e-6);
    }
}

NNH_TEST(sin_cos_of_a_non_finite_angle_is_zero_and_one_and_asserts) {
    const float specials[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()};
    for (float a : specials) {
        g_angle = a;
        CHECK(world_test::asserts_or_is_ignored(sin_cos_of_g_angle));
    }
    if (!nnh::build::kAssertsEnabled) {
        const SinCos sc = sin_cos(std::numeric_limits<float>::quiet_NaN());
        CHECK(sc.sin == 0.0f && sc.cos == 1.0f);
    }
}

NNH_TEST(sin_cos_of_huge_angles_has_defined_values) {
    // 2³⁰ rad: q ≥ 2²³, taken by the integer path; the result is one of the four exact quarter values.
    const float huge[] = {1073741824.0f, -1073741824.0f, 3.0e38f, 16777216.0f * 1.5f, 13176795.0f};
    for (float a : huge) {
        const SinCos sc = sin_cos(a);
        CHECK((sc.sin == 0.0f || sc.sin == 1.0f || sc.sin == -1.0f) && (sc.cos == 0.0f || std::fabs(sc.cos) == 1.0f));
        CHECK(std::fabs(sc.sin) + std::fabs(sc.cos) == 1.0f);
    }
    // Just below 2²³ quarter turns the float path still works (q not yet an integer everywhere).
    const SinCos big = sin_cos(1.0e6f);
    CHECK(std::fabs(big.sin) <= 1.0f && std::fabs(big.cos) <= 1.0f);
}
