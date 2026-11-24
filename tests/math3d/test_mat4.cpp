// 4×4 matrices and 3D transforms (spec 026 FR-1, T1.5): product order, transpose, points and directions, Mat4 × Vec4,
// projective division, to_mat4(Transform3D) = T × R × S, inverses (general and affine), finiteness.

#include <limits>

#include "math3d_fixture.hpp"

using math3d_test::near;
using nnh::Mat4;
using nnh::Vec3;
using nnh::Vec4;

namespace {

Mat4 translation(float x, float y, float z) {
    Mat4 m;
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}
Mat4 scaling(float x, float y, float z) {
    Mat4 m;
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    return m;
}

} // namespace

NNH_TEST(mat4_product_applies_the_right_operand_first) {
    const Mat4 ts = translation(1, 2, 3) * scaling(2, 2, 2); // scale, then move
    CHECK(nnh::transform_point(ts, {1, 1, 1}) == (Vec3{3, 4, 5}));
    const Mat4 st = scaling(2, 2, 2) * translation(1, 2, 3); // move, then scale
    CHECK(nnh::transform_point(st, {1, 1, 1}) == (Vec3{4, 6, 8}));
}

NNH_TEST(mat4_transpose_and_vec4) {
    Mat4 m;
    for (int i = 0; i < 16; ++i) m.m[i] = static_cast<float>(i + 1);
    const Mat4 t = nnh::transpose(m);
    CHECK(t.at(0, 1) == m.at(1, 0) && t.at(3, 2) == m.at(2, 3) && nnh::transpose(t) == m);
    const Vec4 v = m * Vec4{1, 0, 0, 0};
    CHECK(v == (Vec4{1, 2, 3, 4})); // the first column
    CHECK((m * Vec4{0, 0, 0, 1}) == (Vec4{13, 14, 15, 16}));
}

NNH_TEST(mat4_transform_point_divides_by_w) {
    Mat4 m;
    m.m[15] = 2; // w = 2
    CHECK(nnh::transform_point(m, {2, 4, 6}) == (Vec3{1, 2, 3}));
    m.m[15] = 0; // w = 0: no point
    CHECK(nnh::transform_point(m, {2, 4, 6}) == Vec3{});
}

NNH_TEST(transform3d_is_translate_rotate_scale) {
    const nnh::Transform3D t{.position = {10, 0, 0},
                             .rotation = nnh::quat_axis_angle({0, 1, 0}, nnh::kQuarterTurn),
                             .scale = {2, 3, 4}};
    // (1, 0, 0) scaled → (2, 0, 0), turned about +y → (0, 0, −2), moved → (10, 0, −2).
    CHECK(nnh::transform_point(nnh::to_mat4(t), {1, 0, 0}) == (Vec3{10, 0, -2}));
    CHECK(nnh::transform_point(nnh::to_mat4(t), {0, 1, 0}) == (Vec3{10, 3, 0}));
    CHECK(nnh::transform_direction(nnh::to_mat4(t), {0, 0, 1}) == (Vec3{4, 0, 0}));
}

NNH_TEST(mat4_inverse_round_trips) {
    math3d_test::Rng rng;
    for (int i = 0; i < 300; ++i) {
        const nnh::Transform3D t{.position = {rng.range(-50, 50), rng.range(-50, 50), rng.range(-50, 50)},
                                 .rotation = nnh::quat_euler(rng.range(-3, 3), rng.range(-3, 3), rng.range(-3, 3)),
                                 .scale = {rng.range(0.2f, 3), rng.range(0.2f, 3), rng.range(-3, -0.2f)}};
        const Mat4 m = nnh::to_mat4(t);
        Mat4 inv, inv_affine;
        CHECK(nnh::inverse(m, inv) && nnh::inverse_affine(m, inv_affine));
        const Vec3 p{rng.range(-20, 20), rng.range(-20, 20), rng.range(-20, 20)};
        CHECK(near(nnh::transform_point(inv, nnh::transform_point(m, p)), p, 1e-3f));
        CHECK(near(nnh::transform_point(inv_affine, nnh::transform_point(m, p)), p, 1e-3f));
        CHECK(near(m * inv, Mat4{}, 1e-4f));
    }
    // A projection (not affine) inverts with the general inverse.
    const Mat4 proj = nnh::perspective(1.0f, 400.0f / 240, 0.1f, 100);
    Mat4 inv;
    CHECK(nnh::inverse(proj, inv));
    CHECK(near(proj * inv, Mat4{}, 1e-5f));
}

NNH_TEST(mat4_inverse_refuses_singular_and_non_finite) {
    Mat4 out = scaling(7, 7, 7);
    const Mat4 before = out;
    CHECK(!nnh::inverse(scaling(1, 0, 1), out) && out == before);
    CHECK(!nnh::inverse_affine(scaling(1, 1, 0), out) && out == before);
    const float tiny = std::ldexp(1.0f, -40); // det of scaling(tiny, tiny, tiny) = 2⁻¹²⁰: refused
    CHECK(!nnh::inverse(scaling(tiny, tiny, tiny), out) && !nnh::inverse_affine(scaling(tiny, tiny, tiny), out));
    const float small = std::ldexp(1.0f, -30); // 2⁻⁹⁰: accepted
    CHECK(nnh::inverse(scaling(small, small, small), out) && out.m[0] == std::ldexp(1.0f, 30));
    Mat4 nan = Mat4{};
    nan.m[6] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!nnh::inverse(nan, out) && !nnh::inverse_affine(nan, out));
}

NNH_TEST(mat4_is_finite_checks_every_entry) {
    CHECK(nnh::is_finite(Mat4{}));
    for (int i = 0; i < 16; ++i) {
        Mat4 m;
        m.m[i] = (i & 1) != 0 ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
        CHECK(!nnh::is_finite(m));
    }
}
