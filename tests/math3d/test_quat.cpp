// Quaternions (spec 026 FR-1, T1.3–T1.4): axis-angle, Euler order, product order, conjugate, normalise, rotate,
// to_mat4 for any norm, look_rotation.

#include "math3d_fixture.hpp"

using math3d_test::near;
using math3d_test::same_rotation;
using nnh::Quat;
using nnh::Vec3;

NNH_TEST(quat_axis_angle_matches_the_half_angle_formula) {
    const Quat q = nnh::quat_axis_angle({0, 2, 0}, 1.0f); // the axis is normalised
    const nnh::SinCos h = nnh::sin_cos(0.5f);
    CHECK(q == (Quat{0, h.sin, 0, h.cos}));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::quat_axis_angle(Vec3{}, 1.0f) == Quat{}); }));
}

NNH_TEST(quat_product_applies_the_right_operand_first) {
    const Quat yaw = nnh::quat_axis_angle({0, 1, 0}, nnh::kQuarterTurn);
    const Quat pitch = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn);
    // pitch first: −z → +y (nose up), then yaw about +y leaves +y.
    CHECK(nnh::rotate(yaw * pitch, {0, 0, -1}) == (Vec3{0, 1, 0}));
    // yaw first: −z → −x, then pitch about +x leaves −x.
    CHECK(nnh::rotate(pitch * yaw, {0, 0, -1}) == (Vec3{-1, 0, 0}));
}

NNH_TEST(quat_euler_is_yaw_after_pitch_after_roll) {
    const float y = 0.7f, p = -0.4f, r = 1.3f;
    const Quat e = nnh::quat_euler(y, p, r);
    const Quat manual = nnh::quat_axis_angle({0, 1, 0}, y) * nnh::quat_axis_angle({1, 0, 0}, p) *
                        nnh::quat_axis_angle({0, 0, 1}, r);
    CHECK(e == manual);
}

NNH_TEST(quat_conjugate_undoes_a_rotation) {
    const Quat q = nnh::normalize(Quat{0.3f, -0.5f, 0.2f, 0.8f});
    const Vec3 v{1.5f, -2, 0.25f};
    CHECK(near(nnh::rotate(nnh::conjugate(q), nnh::rotate(q, v)), v, 1e-5f));
    CHECK(nnh::conjugate(Quat{1, 2, 3, 4}) == (Quat{-1, -2, -3, 4}));
}

NNH_TEST(quat_normalize_and_its_zero_rule) {
    const Quat n = nnh::normalize(Quat{0, 0, 0, -3});
    CHECK(n == (Quat{0, 0, 0, -1}));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::normalize(Quat{0, 0, 0, 0}) == Quat{}); }));
}

NNH_TEST(quat_to_mat4_ignores_the_norm) {
    const Quat q = nnh::normalize(Quat{0.1f, 0.7f, -0.3f, 0.6f});
    const Quat big{q.x * 4, q.y * 4, q.z * 4, q.w * 4};
    CHECK(near(nnh::to_mat4(q), nnh::to_mat4(big), 1e-6f));
    CHECK(nnh::to_mat4(Quat{0, 0, 0, 0}) == nnh::Mat4{}); // zero: the identity
    const nnh::Mat4 m = nnh::to_mat4(q);
    CHECK(m.at(3, 3) == 1 && m.at(0, 3) == 0 && m.at(3, 0) == 0);
}

NNH_TEST(quat_rotate_matches_the_matrix) {
    const Quat q = nnh::quat_axis_angle({1, 1, 1}, 2.0943952f); // 120° about the diagonal: x → y → z → x
    CHECK(near(nnh::rotate(q, {1, 0, 0}), Vec3{0, 1, 0}, 2e-6f));
    CHECK(nnh::rotate(q, {1, 2, 3}) == nnh::transform_direction(nnh::to_mat4(q), {1, 2, 3}));
}

NNH_TEST(look_rotation_points_minus_z_along_forward) {
    const Vec3 f = nnh::normalize(Vec3{1, -1, -2});
    const Quat q = nnh::look_rotation(f);
    CHECK(near(nnh::rotate(q, {0, 0, -1}), f, 2e-6f));
    const Vec3 up = nnh::rotate(q, {0, 1, 0});
    CHECK(up.y > 0 && near(nnh::dot(up, f), 0, 2e-6f));       // up stays up, perpendicular
    CHECK(near(nnh::rotate(q, {1, 0, 0}).y, 0, 2e-6f));       // no roll: right stays horizontal
    CHECK(same_rotation(nnh::look_rotation({0, 0, -1}), Quat{}, 0));
    CHECK(same_rotation(nnh::look_rotation({0, 0, 1}), Quat{0, 1, 0, 0}, 0)); // turned around
}

NNH_TEST(look_rotation_straight_down_or_up_picks_a_z_up) {
    const Quat down = nnh::look_rotation({0, -1, 0});
    CHECK(near(nnh::rotate(down, {0, 0, -1}), Vec3{0, -1, 0}, 1e-6f));
    CHECK(near(nnh::rotate(down, {1, 0, 0}), Vec3{1, 0, 0}, 1e-6f)); // screen right = +x
    CHECK(near(nnh::rotate(down, {0, 1, 0}), Vec3{0, 0, -1}, 1e-6f)); // screen up = −z (north)
    const Quat up = nnh::look_rotation({0, 3, 0});
    CHECK(near(nnh::rotate(up, {0, 0, -1}), Vec3{0, 1, 0}, 1e-6f));
    CHECK(near(nnh::rotate(up, {1, 0, 0}), Vec3{1, 0, 0}, 1e-6f));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::look_rotation(Vec3{}) == Quat{}); }));
}
