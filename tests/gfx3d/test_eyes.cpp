// Per-eye cameras (spec 026 FR-14, T12.1): one eye at slider 0, for orthographic cameras and with stereo off, the
// same matrices as mono; two eyes moved along the camera's right by half the separation (slider × strength,
// focal / 20 by default) with off-axis projections that put the focal plane at screen depth.

#include <cmath>
#include <cstring>

#include "gfx3d/pass3d.hpp"
#include "gfx3d_fixture.hpp"

using namespace gfx3d_test;
using nnh::Camera3D;
using nnh::Mat4;
using nnh::Vec3;
using nnh::detail::Eyes;
using nnh::detail::make_eyes;

namespace {

bool same_bits(const Mat4& a, const Mat4& b) { return std::memcmp(a.m, b.m, sizeof a.m) == 0; }

/// Where a world point lands in an eye's picture, in pixels (x only).
float pixel_x(const nnh::detail::EyeView& e, Vec3 p) {
    const nnh::Vec4 clip = e.projection * (e.view * nnh::Vec4{p.x, p.y, p.z, 1});
    return (clip.x / clip.w + 1.0f) * 200.0f;
}

} // namespace

NNH_TEST(eyes_one_at_slider_zero_with_the_mono_matrices) {
    Camera3D cam;
    cam.position = {1, 2, 6};
    nnh::gfx3d::point_at(cam, {0, 0, 0});
    const Eyes e = make_eyes(cam, 0.0f);
    CHECK(e.count == 1 && e.eye[0].offset == 0.0f);
    CHECK(same_bits(e.eye[0].view, nnh::detail::camera_view(cam)));
    CHECK(same_bits(e.eye[0].projection, nnh::detail::camera_projection(cam)));
}

NNH_TEST(eyes_one_for_orthographic_cameras_and_with_stereo_off) {
    Camera3D ortho;
    ortho.projection = nnh::Projection::Orthographic;
    const Eyes o = make_eyes(ortho, 1.0f);
    CHECK(o.count == 1 && same_bits(o.eye[0].projection, nnh::detail::camera_projection(ortho)));
    Camera3D off;
    off.stereo.enabled = false;
    const Eyes f = make_eyes(off, 1.0f);
    CHECK(f.count == 1 && same_bits(f.eye[0].view, nnh::detail::camera_view(off)));
}

NNH_TEST(eyes_two_half_a_separation_apart) {
    Camera3D cam; // focal 5, strength 0 = focal / 20 = 0.25 at slider 1
    const Eyes e = make_eyes(cam, 1.0f);
    CHECK(e.count == 2 && e.eye[0].offset == -0.125f && e.eye[1].offset == 0.125f);
    const Eyes half = make_eyes(cam, 0.5f);
    CHECK(half.eye[1].offset == 0.0625f);
    cam.stereo.strength = 0.5f;
    CHECK(make_eyes(cam, 0.5f).eye[1].offset == 0.125f);
}

NNH_TEST(eyes_move_along_the_cameras_right) {
    Camera3D cam;
    cam.position = {10, 0, 0};
    nnh::gfx3d::point_at(cam, {0, 0, 0}); // looking down −x: its right is −z
    const Eyes e = make_eyes(cam, 1.0f);
    const Vec3 right_eye = nnh::transform_point(e.eye[1].view, {10, 0, -0.125f});
    const Vec3 left_eye = nnh::transform_point(e.eye[0].view, {10, 0, 0.125f});
    CHECK(std::fabs(right_eye.x) < 1e-6f && std::fabs(right_eye.z) < 1e-6f);
    CHECK(std::fabs(left_eye.x) < 1e-6f && std::fabs(left_eye.z) < 1e-6f);
}

NNH_TEST(eyes_put_the_focal_plane_at_screen_depth) {
    Camera3D cam; // at (0, 0, 5) looking down −z: the focal plane (5 ahead) is z = 0
    const Eyes e = make_eyes(cam, 1.0f);
    for (const float x : {-1.5f, 0.0f, 0.7f, 2.0f}) {
        CHECK(std::fabs(pixel_x(e.eye[0], {x, 0.3f, 0}) - pixel_x(e.eye[1], {x, 0.3f, 0})) < 1e-3f);
    }
    // Nearer points: crossed (the right eye sees them further left), in front of the screen; farther ones behind.
    CHECK(pixel_x(e.eye[1], {0, 0, 3}) < pixel_x(e.eye[0], {0, 0, 3}) - 1.0f);
    CHECK(pixel_x(e.eye[1], {0, 0, -20}) > pixel_x(e.eye[0], {0, 0, -20}) + 1.0f);
    // About 10 pixels apart at infinity with a 60° view at slider 1 (plan P12).
    const float far = pixel_x(e.eye[1], {0, 0, -1e6f}) - pixel_x(e.eye[0], {0, 0, -1e6f});
    CHECK(far > 10.0f && far < 11.0f);
}

NNH_TEST(eyes_centre_projection_is_the_mono_one_but_for_the_shift) {
    Camera3D cam;
    const Eyes e = make_eyes(cam, 1.0f);
    const Mat4 mono = nnh::detail::camera_projection(cam);
    for (int i = 0; i < 16; ++i) {
        if (i == 8) continue;
        CHECK(e.eye[0].projection.m[i] == mono.m[i] && e.eye[1].projection.m[i] == mono.m[i]);
    }
    CHECK(e.eye[0].projection.m[8] == -e.eye[1].projection.m[8] && e.eye[1].projection.m[8] < 0);
}

NNH_TEST(eyes_invalid_stereo_settings_are_refused) {
    restart();
    CHECK(asserts_or_is_ignored([] {
        Camera3D bad;
        bad.stereo.focal_distance = 0;
        nnh::gfx3d::set_camera(nnh::Screen::Top, bad);
    }));
    CHECK(asserts_or_is_ignored([] {
        Camera3D bad;
        bad.stereo.strength = -1;
        nnh::gfx3d::set_camera(nnh::Screen::Top, bad);
    }));
    CHECK(nnh::gfx3d::camera(nnh::Screen::Top).stereo.focal_distance == 5.0f);
    nnh::shutdown();
}
