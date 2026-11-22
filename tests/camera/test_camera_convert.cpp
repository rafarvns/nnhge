// Screen ↔ world conversions and pixel snap (spec 018 FR-11, FR-13): round trips for both screens, both layouts
// and zooms 0.5, 1, 2, 3; pixel snap rounds the applied camera only.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraLayout;
using nnh::CameraRig;
using nnh::Screen;
using nnh::Vec2;
namespace cam = nnh::detail::cam;

namespace {

bool near(Vec2 a, Vec2 b, float tolerance) {
    const Vec2 d = a - b;
    return d.x <= tolerance && -d.x <= tolerance && d.y <= tolerance && -d.y <= tolerance;
}

} // namespace

NNH_TEST(conversions_round_trip_on_every_screen_layout_and_zoom) {
    const float zooms[] = {0.5f, 1, 2, 3};
    const CameraLayout layouts[] = {CameraLayout::Single, CameraLayout::TallWorld};
    const Vec2 points[] = {{0, 0}, {399, 239}, {123.25f, 77.5f}, {319, 0}};
    for (CameraLayout layout : layouts) {
        for (Screen screen : {Screen::Top, Screen::Bottom}) {
            for (float z : zooms) {
                CameraRig rig({.screen = screen, .layout = layout, .zoom = z, .gap = 12});
                rig.snap(Vec2{1234.5f, 678.25f});
                for (Vec2 p : points) {
                    const Vec2 w = rig.to_world(screen, p);
                    CHECK(near(rig.to_screen(screen, w), p, 1.0f / 256));
                    // The world point under p is camera + p / zoom.
                    CHECK(near(w, rig.camera(screen).position + p / z, 1.0f / 1024));
                }
            }
        }
    }
}

NNH_TEST(conversions_at_power_of_two_zooms_are_exact) {
    CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = 2});
    rig.snap(Vec2{1000.5f, 300.25f});
    const Vec2 w = rig.to_world(Screen::Bottom, {17, 33});
    CHECK(at(rig.to_screen(Screen::Bottom, w), 17, 33));
}

NNH_TEST(view_is_the_world_rectangle_each_screen_shows) {
    CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = 2});
    rig.snap(Vec2{1000, 1000});
    const nnh::Rect top = rig.view(Screen::Top);
    const nnh::Rect bottom = rig.view(Screen::Bottom);
    CHECK(top.x == 900 && top.y == 880 && top.w == 200 && top.h == 120);
    CHECK(bottom.x == 920 && bottom.y == 1000 && bottom.w == 160 && bottom.h == 120);
}

NNH_TEST(pixel_snap_rounds_the_applied_camera_to_screen_pixels_only) {
    CameraRig rig({.pixel_snap = true, .zoom = 2});
    rig.snap(Vec2{1000.3f, 500.8f}); // top-left (900.3, 440.8): 1800.6, 881.6 screen pixels
    const Vec2 c = rig.camera(Screen::Top).position;
    CHECK(at(c, 900.5f, 441));
    CHECK(!(rig.position() == c) && !(rig.followed() == c)); // the reported positions are never rounded
    CHECK(at(rig.to_screen(Screen::Top, c), 0, 0)); // conversions use the snapped camera
}

NNH_TEST(pixel_snap_is_off_by_default) {
    CameraRig rig;
    rig.snap(Vec2{1000.3f, 500.8f});
    CHECK(cam_test::same_bits(rig.camera(Screen::Top).position, rig.position()));
}

NNH_TEST(round_half_up_is_floor_of_v_plus_a_half) {
    CHECK(cam::round_half_up(1.5f) == 2 && cam::round_half_up(1.49f) == 1 && cam::round_half_up(-1.5f) == -1);
    CHECK(cam::round_half_up(-1.51f) == -2 && cam::round_half_up(-0.2f) == 0 && cam::round_half_up(7) == 7);
    CHECK(cam::round_half_up(-7) == -7 && cam::round_half_up(1e9f) == 1e9f);
}
