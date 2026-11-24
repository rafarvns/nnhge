// Projections and look-at (spec 026 FR-1/FR-2, T1.6): −z forward, near → −1 and far → +1, the field of view's edge on
// the clip boundary, orthographic extents, invalid values refused.

#include "math3d_fixture.hpp"

using math3d_test::near;
using nnh::Mat4;
using nnh::Vec3;

NNH_TEST(perspective_maps_near_and_far_to_minus_one_and_one) {
    const Mat4 p = nnh::perspective(1.0471976f, 400.0f / 240.0f, 0.5f, 50);
    CHECK(near(nnh::transform_point(p, {0, 0, -0.5f}).z, -1, 1e-6f));
    CHECK(near(nnh::transform_point(p, {0, 0, -50}).z, 1, 1e-6f));
    // The top of the view at distance d is d·tan(30°): it lands on y = 1.
    const float d = 10, top = d * 0.57735027f;
    CHECK(near(nnh::transform_point(p, {0, top, -d}).y, 1, 1e-5f));
    CHECK(near(nnh::transform_point(p, {top * 400 / 240, 0, -d}).x, 1, 1e-5f));
    CHECK(p.at(3, 2) == -1 && p.at(3, 3) == 0);
}

NNH_TEST(perspective_of_a_right_angle_is_exact) {
    // fov 90°: tan 45° = s/c with s = c exactly → focal length 1.
    const Mat4 p = nnh::perspective(nnh::kQuarterTurn, 1, 1, 3);
    CHECK(p.at(0, 0) == 1 && p.at(1, 1) == 1);
    CHECK(p.at(2, 2) == -2 && p.at(2, 3) == -3); // (f + n)/(n − f), 2fn/(n − f)
}

NNH_TEST(orthographic_maps_its_box_to_the_unit_cube) {
    const Mat4 o = nnh::orthographic(10, 2, 1, 21);
    CHECK(near(nnh::transform_point(o, {10, 5, -1}), Vec3{1, 1, -1}, 1e-6f));
    CHECK(near(nnh::transform_point(o, {-10, -5, -21}), Vec3{-1, -1, 1}, 1e-6f));
    CHECK(o.at(3, 3) == 1 && o.at(3, 2) == 0);
}

NNH_TEST(projections_refuse_invalid_values) {
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::perspective(0, 1, 0.1f, 10) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::perspective(1, 1, 0, 10) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::perspective(1, 1, 5, 5) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::perspective(1, 0, 0.1f, 10) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::perspective(3.5f, 1, 0.1f, 10) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::orthographic(0, 1, 0, 10) == Mat4{}); }));
    CHECK(math3d_test::asserts_or_is_ignored([] { CHECK(nnh::orthographic(1, 1, 3, 3) == Mat4{}); }));
}

NNH_TEST(look_at_builds_a_rigid_view) {
    const Mat4 v = nnh::look_at({3, 4, 5}, {-1, 0, 2}, {0, 1, 0});
    CHECK(near(nnh::transform_point(v, {3, 4, 5}), Vec3{}, 1e-5f)); // the eye at the origin
    const Vec3 t = nnh::transform_point(v, {-1, 0, 2});
    CHECK(near(t.x, 0, 1e-5f) && near(t.y, 0, 1e-5f) && t.z < 0); // the target straight ahead on −z
    CHECK(near(-t.z, nnh::length(Vec3{4, 4, 3}), 1e-5f));
    Mat4 inv;
    CHECK(nnh::inverse_affine(v, inv));
    CHECK(near(nnh::transform_point(inv, Vec3{}), Vec3{3, 4, 5}, 1e-5f));
}

NNH_TEST(look_at_straight_down_keeps_north_up) {
    const Mat4 v = nnh::look_at({0, 10, 0}, {0, 0, 0});
    CHECK(near(nnh::transform_point(v, {0, 0, -1}), Vec3{0, 1, -10}, 1e-6f)); // −z (north) is screen up
    CHECK(near(nnh::transform_point(v, {1, 0, 0}), Vec3{1, 0, -10}, 1e-6f));
}
