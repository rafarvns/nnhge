// 3D vectors (spec 026 FR-1, T1.1–T1.2): layout, defaults, operators, dot, cross, length, normalise with the 2⁻¹⁰⁰
// rule (an exponent test) and its misuse assert.

#include <limits>
#include <type_traits>

#include "math3d_fixture.hpp"

using math3d_test::bits;
using nnh::Vec3;

NNH_TEST(math3d_values_are_plain_and_compact) {
    static_assert(std::is_trivially_copyable_v<Vec3> && sizeof(Vec3) == 12);
    static_assert(std::is_trivially_copyable_v<nnh::Vec4> && sizeof(nnh::Vec4) == 16);
    static_assert(std::is_trivially_copyable_v<nnh::Quat> && sizeof(nnh::Quat) == 16);
    static_assert(std::is_trivially_copyable_v<nnh::Mat4> && sizeof(nnh::Mat4) == 64);
    static_assert(std::is_trivially_copyable_v<nnh::Transform3D> && sizeof(nnh::Transform3D) == 40);
    static_assert(std::is_trivially_copyable_v<nnh::Ray3>);
    CHECK(Vec3{} == (Vec3{0, 0, 0}));
    CHECK(nnh::Quat{} == (nnh::Quat{0, 0, 0, 1}));
    CHECK(nnh::Mat4{} == nnh::Mat4::identity());
    CHECK(nnh::Mat4{}.at(0, 0) == 1 && nnh::Mat4{}.at(3, 3) == 1 && nnh::Mat4{}.at(1, 0) == 0);
    const nnh::Transform3D t{};
    CHECK(t.scale == (Vec3{1, 1, 1}) && t.position == Vec3{});
    CHECK(nnh::Ray3{}.direction == (Vec3{0, 0, -1}));
}

NNH_TEST(vec3_operators_are_component_wise) {
    Vec3 a{1, 2, 3};
    CHECK((a + Vec3{1, 1, 1}) == (Vec3{2, 3, 4}));
    CHECK((a - Vec3{1, 1, 1}) == (Vec3{0, 1, 2}));
    CHECK(-a == (Vec3{-1, -2, -3}));
    CHECK(a * 2.0f == (Vec3{2, 4, 6}) && 2.0f * a == (Vec3{2, 4, 6}));
    CHECK(a / 2.0f == (Vec3{0.5f, 1, 1.5f}));
    a += Vec3{1, 0, 0};
    a -= Vec3{0, 1, 0};
    a *= 3.0f;
    CHECK(a == (Vec3{6, 3, 9}));
}

NNH_TEST(vec3_dot_cross_and_length) {
    CHECK(nnh::dot({1, 2, 3}, {4, -5, 6}) == 12.0f);
    CHECK(nnh::cross({1, 0, 0}, {0, 1, 0}) == (Vec3{0, 0, 1}));
    CHECK(nnh::cross({0, 1, 0}, {0, 0, 1}) == (Vec3{1, 0, 0}));
    CHECK(nnh::cross({0, 0, 1}, {1, 0, 0}) == (Vec3{0, 1, 0}));
    CHECK(nnh::cross({2, 3, 4}, {5, 6, 7}) == (Vec3{-3, 6, -3}));
    CHECK(nnh::length_squared({2, 3, 6}) == 49.0f);
    CHECK(nnh::length({2, 3, 6}) == 7.0f);
}

NNH_TEST(vec3_normalize_keeps_axes_exact) {
    CHECK(nnh::normalize(Vec3{0, 0, -5}) == (Vec3{0, 0, -1}));
    CHECK(nnh::normalize(Vec3{3, 0, 0}) == (Vec3{1, 0, 0}));
    CHECK(nnh::normalize(Vec3{2, 3, 6}) == (Vec3{2.0f / 7, 3.0f / 7, 6.0f / 7}));
    CHECK(math3d_test::asserts_or_is_ignored([] { // length² 9e-40, below 2⁻¹⁰⁰: zero
        CHECK(nnh::normalize(Vec3{1e-20f, 2e-20f, -2e-20f}) == Vec3{});
    }));
    const Vec3 m = nnh::normalize(Vec3{1e-12f, 0, 0}); // length² 1e-24 ≈ 2⁻⁷⁹.7: kept
    CHECK(m == (Vec3{1, 0, 0}));
}

NNH_TEST(vec3_normalize_of_zero_asserts_and_gives_zero) {
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::normalize(Vec3{}) == Vec3{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] {
        const float inf = std::numeric_limits<float>::infinity();
        CHECK(nnh::normalize(Vec3{inf, 0, 0}) == Vec3{});
    }));
}

NNH_TEST(vec3_normalize_threshold_is_two_to_the_minus_100) {
    // length² exactly 2⁻¹⁰⁰ is kept; just below is zero (an exponent test, the same on every system).
    const float at = std::ldexp(1.0f, -50); // squared: 2⁻¹⁰⁰
    CHECK(nnh::normalize(Vec3{at, 0, 0}) == (Vec3{1, 0, 0}));
    CHECK(math3d_test::asserts_or_is_ignored([] {
        const float below = std::nextafter(std::ldexp(1.0f, -50), 0.0f);
        CHECK(nnh::normalize(Vec3{below, 0, 0}) == Vec3{});
    }));
    CHECK(bits(nnh::normalize(Vec3{0, -2, 0}).y) == bits(-1.0f));
}
