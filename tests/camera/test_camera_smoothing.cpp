// Smoothing (spec 018 FR-4): a fixed fraction of the remaining distance per frame, per axis; closer than 1/64 px
// it snaps, so a still target leaves the camera exactly on the desired bits; fraction 1 is an assignment.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using cam_test::same_bits;
using nnh::CameraRig;
using nnh::Vec2;
namespace cam = nnh::detail::cam;

NNH_TEST(smoothing_moves_a_fixed_fraction_of_the_remaining_distance) {
    CameraRig rig({.smoothing = {0.5f, 0.25f}});
    rig.update(Vec2{200 + 64, 120 + 64}); // desired (64, 64) from (0, 0)
    CHECK(at(rig.position(), 32, 16));
    rig.update(Vec2{200 + 64, 120 + 64});
    CHECK(at(rig.position(), 48, 28));
}

NNH_TEST(smoothing_fraction_table) {
    const float fractions[] = {0.1f, 0.2f, 0.5f, 0.75f, 0.9f};
    for (float f : fractions) {
        CHECK(cam::smooth_axis(10, 110, f) == 10 + 100 * f);
    }
}

NNH_TEST(smoothing_snaps_below_a_64th_of_a_pixel) {
    const float eps = 1.0f / 1024;
    CHECK(cam::smooth_axis(0, nnh::kCameraSnapDistance - eps, 0.5f) == nnh::kCameraSnapDistance - eps);
    CHECK(cam::smooth_axis(0, -(nnh::kCameraSnapDistance - eps), 0.5f) == -(nnh::kCameraSnapDistance - eps));
    CHECK(cam::smooth_axis(0, nnh::kCameraSnapDistance + eps, 0.5f) == (nnh::kCameraSnapDistance + eps) * 0.5f);
    CHECK(cam::smooth_axis(0, nnh::kCameraSnapDistance, 0.5f) == nnh::kCameraSnapDistance * 0.5f);
}

NNH_TEST(a_still_target_settles_to_the_exact_bits_of_the_desired_position) {
    CameraRig smooth({.smoothing = {0.13f, 0.07f}});
    CameraRig instant;
    const Vec2 target{1234.567f, -89.123f};
    instant.update(target);
    cam_test::run(smooth, target, 600);
    CHECK(same_bits(smooth.position(), instant.position()));
    const Vec2 settled = smooth.position();
    cam_test::run(smooth, target, 10);
    CHECK(same_bits(smooth.position(), settled)); // and stays still, bit for bit
}

NNH_TEST(fraction_1_is_an_assignment_even_where_adding_the_difference_would_round) {
    // followed + (desired − followed) is not always desired: 1e8 + (0.5 − 1e8) = 0, not 0.5.
    const float followed = 1e8f, desired = 0.5f;
    CHECK(followed + (desired - followed) != desired);
    CHECK(cam::smooth_axis(followed, desired, 1) == desired);
}

NNH_TEST(smoothing_is_per_axis) {
    CameraRig rig({.smoothing = {1, 0.5f}});
    rig.update(Vec2{300, 220}); // desired (100, 100)
    CHECK(at(rig.position(), 100, 50));
}
