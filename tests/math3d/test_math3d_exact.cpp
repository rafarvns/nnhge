// Exact cases (spec 026 FR-3, T1.3/T1.5): identities, quarter, half and three-quarter turns about each axis map axes
// to axes exactly, compositions of quarter turns too, translations add once per axis.

#include "math3d_fixture.hpp"

using nnh::Mat4;
using nnh::Quat;
using nnh::Vec3;

namespace {

/// Every entry of the 3×3 part is exactly 0, 1 or −1.
bool is_signed_permutation(const Mat4& m) {
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) {
            const float v = m.at(r, c);
            if (v != 0.0f && v != 1.0f && v != -1.0f) return false;
        }
    return true;
}

} // namespace

NNH_TEST(exact_sin_cos_of_an_eighth_turn_has_equal_bits_after_the_snap) {
    // The half angle of a quarter turn: quat_axis_angle makes |sin| and |cos| bit-identical (plan P4).
    const Quat q = nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn);
    CHECK(math3d_test::bits(q.z) == math3d_test::bits(q.w));
    const Quat t = nnh::quat_axis_angle({0, 0, 1}, nnh::kThreeQuarterTurn);
    CHECK(math3d_test::bits(t.z) == math3d_test::bits(t.w) || math3d_test::bits(t.z) == math3d_test::bits(-t.w));
}

NNH_TEST(exact_quarter_turns_map_axes_to_axes) {
    const Vec3 axes[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const float angles[6] = {nnh::kQuarterTurn,      nnh::kHalfTurn,      nnh::kThreeQuarterTurn,
                             -nnh::kQuarterTurn,     -nnh::kHalfTurn,     nnh::kFullTurn};
    for (const Vec3& axis : axes)
        for (const float a : angles) {
            const Quat q = nnh::quat_axis_angle(axis, a);
            CHECK(is_signed_permutation(nnh::to_mat4(q)));
            for (const Vec3& v : axes) {
                const Vec3 r = nnh::rotate(q, v);
                CHECK(nnh::dot(r, r) == 1.0f);
            }
        }
    // Spot checks with signs.
    const Quat z90 = nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn);
    CHECK(nnh::rotate(z90, {1, 0, 0}) == (Vec3{0, 1, 0}));
    CHECK(nnh::rotate(z90, {0, 1, 0}) == (Vec3{-1, 0, 0}));
    const Quat x180 = nnh::quat_axis_angle({1, 0, 0}, nnh::kHalfTurn);
    CHECK(nnh::rotate(x180, {0, 1, 0}) == (Vec3{0, -1, 0}));
    const Quat y270 = nnh::quat_axis_angle({0, 1, 0}, nnh::kThreeQuarterTurn);
    CHECK(nnh::rotate(y270, {1, 0, 0}) == (Vec3{0, 0, 1}));
}

NNH_TEST(exact_compositions_of_quarter_turns_stay_exact) {
    const Quat q = nnh::quat_euler(nnh::kQuarterTurn, nnh::kQuarterTurn, -nnh::kQuarterTurn);
    CHECK(is_signed_permutation(nnh::to_mat4(q)));
    const Quat chain = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn) *
                       nnh::quat_axis_angle({0, 1, 0}, nnh::kHalfTurn) *
                       nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn);
    CHECK(is_signed_permutation(nnh::to_mat4(chain)));
}

NNH_TEST(exact_identities) {
    CHECK(nnh::to_mat4(Quat{}) == Mat4{});
    CHECK(nnh::to_mat4(nnh::Transform3D{}) == Mat4{});
    CHECK(Mat4{} * Mat4{} == Mat4{});
    Mat4 inv;
    CHECK(nnh::inverse(Mat4{}, inv) && inv == Mat4{});
    CHECK(nnh::inverse_affine(Mat4{}, inv) && inv == Mat4{});
    const Vec3 p{1.25f, -7, 3e5f};
    CHECK(nnh::transform_point(Mat4{}, p) == p);
    CHECK(nnh::rotate(Quat{}, p) == p);
}

NNH_TEST(exact_translation_adds_once_per_axis) {
    const nnh::Transform3D t{.position = {0.1f, -0.2f, 1e-3f}};
    const Vec3 p{3.3f, 1e4f, -7.77f};
    const Vec3 q = nnh::transform_point(nnh::to_mat4(t), p);
    CHECK(q == (Vec3{p.x + 0.1f, p.y + -0.2f, p.z + 1e-3f}));
}
