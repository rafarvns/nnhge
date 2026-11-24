// Points, directions, inverses and finiteness (spec 022 FR-2, T1.4): round trips, a zero scale refused, tiny but
// normal determinants accepted, the 2⁻¹⁰⁰ threshold by exponent, directions without translation, every special float.

#include "nnh/transform.hpp"
#include "test.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

using nnh::Affine2D;
using nnh::Transform2D;
using nnh::Vec2;

namespace {

bool near(Vec2 a, Vec2 b, float eps) { return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps; }
std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

} // namespace

NNH_TEST(affine_point_and_direction) {
    const Affine2D m = nnh::to_world({.position = {10, 20}, .rotation = nnh::kQuarterTurn, .scale = {2, 2}}).map;
    CHECK(nnh::transform_point(m, {3, 0}) == (Vec2{10, 26}));
    CHECK(nnh::transform_direction(m, {3, 0}) == (Vec2{0, 6})); // no translation
    CHECK(nnh::transform_point(m, {}) == (Vec2{10, 20}));
}

NNH_TEST(affine_inverse_round_trips) {
    for (int i = 0; i < 200; ++i) {
        const float f = static_cast<float>(i);
        const Affine2D m = nnh::compose(nnh::to_world({.position = {f * 3 - 200, 50 - f}, .rotation = f * 0.07f,
                                                       .scale = {0.5f + f / 100, -1.25f}}),
                                        {.position = {f, -f}, .rotation = -f * 0.11f, .scale = {2.0f, 0.75f + f / 300}})
                               .map;
        Affine2D inv;
        CHECK(nnh::inverse(m, inv));
        const Vec2 p{f * 1.5f - 100, 33 - f};
        CHECK(near(nnh::transform_point(inv, nnh::transform_point(m, p)), p, 1e-3f));
        Vec2 local{};
        CHECK(nnh::to_local(m, nnh::transform_point(m, p), local));
        CHECK(near(local, p, 1e-3f));
    }
}

NNH_TEST(affine_inverse_of_the_identity_is_the_identity) {
    Affine2D inv;
    CHECK(nnh::inverse(Affine2D{}, inv));
    CHECK(inv.x == (Vec2{1, 0}) && inv.y == (Vec2{0, 1}) && inv.origin == Vec2{});
}

NNH_TEST(affine_zero_scale_is_refused_and_out_untouched) {
    const Affine2D m = nnh::to_world({.position = {1, 2}, .scale = {0, 1}}).map;
    Affine2D out{{7, 7}, {7, 7}, {7, 7}};
    CHECK(!nnh::inverse(m, out));
    CHECK(out.x == (Vec2{7, 7}) && out.origin == (Vec2{7, 7}));
    Vec2 p{9, 9};
    CHECK(!nnh::to_local(m, {3, 3}, p) && p == (Vec2{9, 9}));
}

NNH_TEST(affine_threshold_is_two_to_the_minus_100) {
    // det = s²: 2⁻⁶⁰ squared is 2⁻¹²⁰ (refused); 2⁻⁴⁰ squared is 2⁻⁸⁰ (accepted); 2⁻⁵⁰ squared is exactly 2⁻¹⁰⁰
    // (accepted: the refusal is "below").
    const float s60 = std::ldexp(1.0f, -60), s40 = std::ldexp(1.0f, -40), s50 = std::ldexp(1.0f, -50);
    Affine2D out;
    CHECK(!nnh::inverse(nnh::to_world({.scale = {s60, s60}}).map, out));
    CHECK(nnh::inverse(nnh::to_world({.scale = {s40, s40}}).map, out));
    CHECK(nnh::inverse(nnh::to_world({.scale = {s50, s50}}).map, out));
    // A 2⁻⁶⁰ scale on one axis only (det 2⁻⁶⁰) is accepted: tiny, normal, invertible.
    CHECK(nnh::inverse(nnh::to_world({.scale = {s60, 1}}).map, out));
}

NNH_TEST(affine_non_finite_maps_are_refused) {
    const float inf = std::numeric_limits<float>::infinity(), nan = std::numeric_limits<float>::quiet_NaN();
    Affine2D out;
    CHECK(!nnh::inverse({{inf, 0}, {0, 1}, {}}, out));
    CHECK(!nnh::inverse({{nan, 0}, {0, 1}, {}}, out));
    CHECK(!nnh::inverse({{1e30f, 0}, {0, 1e30f}, {}}, out)); // det overflows to infinity
}

NNH_TEST(affine_is_finite_tests_every_special_pattern) {
    const float specials[] = {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN(), std::bit_cast<float>(0xFFC00001u),
                              std::bit_cast<float>(0x7F800001u)};
    CHECK(nnh::is_finite(Transform2D{}));
    CHECK(nnh::is_finite({.position = {std::numeric_limits<float>::max(), -0.0f},
                          .rotation = std::numeric_limits<float>::denorm_min(), .scale = {-1e-38f, 3e38f}}));
    for (float v : specials) {
        for (int field = 0; field < 5; ++field) {
            Transform2D t;
            float* f[] = {&t.position.x, &t.position.y, &t.rotation, &t.scale.x, &t.scale.y};
            *f[field] = v;
            CHECK(!nnh::is_finite(t));
        }
    }
    CHECK(bits(1.0f) == 0x3F800000u);
}
