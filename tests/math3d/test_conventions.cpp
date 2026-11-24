// Conventions (spec 026 FR-2, T1.2/T1.6): right-handed, +y up, a camera looks down −z, column vectors, column-major
// storage, radians.

#include "math3d_fixture.hpp"

using math3d_test::near;
using nnh::Mat4;
using nnh::Vec3;

NNH_TEST(conventions_right_handed) {
    CHECK(nnh::cross({1, 0, 0}, {0, 1, 0}) == (Vec3{0, 0, 1})); // x × y = z
}

NNH_TEST(conventions_matrices_are_column_major_on_column_vectors) {
    Mat4 t; // a translation by (5, 6, 7) lives in the last column: m[12..14]
    t.m[12] = 5;
    t.m[13] = 6;
    t.m[14] = 7;
    CHECK(t.at(0, 3) == 5 && t.at(1, 3) == 6 && t.at(2, 3) == 7);
    CHECK(nnh::transform_point(t, {1, 2, 3}) == (Vec3{6, 8, 10}));
    CHECK(nnh::transform_direction(t, {1, 2, 3}) == (Vec3{1, 2, 3}));
}

NNH_TEST(conventions_positive_angles_turn_counter_clockwise_about_the_axis) {
    // Radians; seen from +z looking at the origin, a positive turn about +z takes +x to +y.
    const Vec3 v = nnh::rotate(nnh::quat_axis_angle({0, 0, 1}, nnh::kQuarterTurn), {1, 0, 0});
    CHECK(v == (Vec3{0, 1, 0}));
}

NNH_TEST(conventions_look_at_points_down_minus_z) {
    // A camera at (0, 0, 5) looking at the origin: the origin is 5 units in front, on −z in camera space.
    const Mat4 view = nnh::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    CHECK(near(nnh::transform_point(view, {0, 0, 0}), Vec3{0, 0, -5}, 0));
    CHECK(near(nnh::transform_point(view, {0, 1, 0}), Vec3{0, 1, -5}, 0)); // +y stays up
    CHECK(near(nnh::transform_point(view, {1, 0, 0}), Vec3{1, 0, -5}, 0)); // +x stays right
    // The default orientation (identity) also looks down −z.
    CHECK(nnh::rotate(nnh::look_rotation({0, 0, -1}), {0, 0, -1}) == (Vec3{0, 0, -1}));
}
