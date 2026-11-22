// Zoom in the rig (spec 018 FR-9, FR-3): the view is screen size / zoom, a zoom change keeps the view's centre,
// the dead zone is in screen pixels, the limits use the zoomed view.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Vec2;
namespace cam = nnh::detail::cam;

NNH_TEST(the_view_is_the_screen_size_divided_by_the_zoom) {
    CHECK(cam::view_size({.zoom = 2}) == (Vec2{200, 120}));
    CHECK(cam::view_size({.zoom = 0.5f}) == (Vec2{800, 480}));
    CHECK(cam::view_size({.screen = nnh::Screen::Bottom, .zoom = 4}) == (Vec2{80, 60}));
    CameraRig rig({.zoom = 2});
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.position(), 900, 440)); // half view (100, 60)
}

NNH_TEST(a_zoom_change_keeps_the_view_centre) {
    const float zooms[] = {0.5f, 0.75f, 1.3f, 2, 3, 4, 1};
    CameraRig rig;
    rig.snap(Vec2{1000.3f, 500.7f});
    for (float z : zooms) {
        const Vec2 before = rig.position() + cam::view_size(rig.settings()) * 0.5f;
        rig.set_zoom(z);
        const Vec2 after = rig.position() + cam::view_size(rig.settings()) * 0.5f;
        CHECK(rig.settings().zoom == z);
        // Within one ulp of 1000 / 500 (2^-13 .. 2^-14): a single rounding of the centre.
        CHECK(after.x - before.x < 0.0002f && before.x - after.x < 0.0002f);
        CHECK(after.y - before.y < 0.0001f && before.y - after.y < 0.0001f);
    }
}

NNH_TEST(set_with_a_new_zoom_keeps_the_centre_too) {
    CameraRig rig;
    rig.snap(Vec2{1000, 500});
    CameraSettings s = rig.settings();
    s.zoom = 2;
    rig.set(s);
    CHECK(at(rig.position(), 900, 440));
    CHECK(at(rig.followed(), 900, 440));
}

NNH_TEST(the_dead_zone_is_in_screen_pixels_so_it_looks_the_same_at_any_zoom) {
    const float zooms[] = {0.5f, 1, 2, 4};
    for (float z : zooms) {
        CameraRig rig({.dead_zone = CameraSettings::centered_zone(64, 48), .zoom = z});
        rig.snap(Vec2{1000, 500});
        const Vec2 start = rig.position();
        rig.update(Vec2{1000 + 32 / z, 500}); // the zone's right edge on screen
        CHECK(rig.position().x == start.x);
        rig.update(Vec2{1000 + 40 / z, 500}); // 8 screen pixels past it
        CHECK(rig.position().x == start.x + 8 / z);
    }
}

NNH_TEST(limits_use_the_zoomed_view) {
    CameraRig rig({.zoom = 2});
    rig.set_limits({0, 0, 2048, 320});
    rig.update(Vec2{5000, 5000});
    CHECK(at(rig.position(), 1848, 200)); // 2048 − 200, 320 − 120
    rig.set_zoom(0.5f);
    rig.update(Vec2{5000, 5000});
    CHECK(at(rig.position(), 1248, -80)); // 2048 − 800; 480 tall view over 320: centred
}
