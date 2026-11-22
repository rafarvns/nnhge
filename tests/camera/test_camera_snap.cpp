// snap() (spec 018 FR-5): the target centred at once (plus the offset), limits kept, dead zone and smoothing
// ignored; the shake is kept (stop_shake clears it).

#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Rect;
using nnh::Vec2;

NNH_TEST(snap_centres_the_target_plus_the_offset_without_smoothing) {
    CameraRig rig({.offset = {10, -4}, .smoothing = {0.1f, 0.1f}});
    rig.snap(Vec2{1000, 500});
    CHECK(at(rig.position(), 810, 376));
    CHECK(at(rig.followed(), 810, 376));
}

NNH_TEST(snap_keeps_the_limits) {
    CameraRig rig;
    rig.set_limits({0, 0, 2048, 320});
    rig.snap(Rect{2040, 300, 10, 14});
    CHECK(at(rig.position(), 1648, 80));
}

NNH_TEST(snap_ignores_the_dead_zone_and_centres_the_target) {
    CameraRig rig({.dead_zone = CameraSettings::centered_zone(100, 100)});
    rig.snap(Vec2{1000, 500});
    rig.snap(Vec2{1030, 520}); // inside the zone, but snap centres anyway
    CHECK(at(rig.position(), 830, 400));
}

NNH_TEST(snap_places_an_axis_that_is_not_followed) {
    CameraRig rig({.follow_y = false});
    rig.snap(Vec2{1000, 500});
    CHECK(at(rig.position(), 800, 380));
}

NNH_TEST(snap_keeps_the_shake_and_stop_shake_clears_it) {
    CameraRig rig;
    rig.add_trauma(1);
    rig.update(Vec2{1000, 500});
    CHECK(!(rig.position() == rig.followed()));
    const float trauma = rig.trauma();
    rig.snap(Vec2{3000, 3000});
    CHECK(rig.trauma() == trauma); // snap doesn't decay trauma
    CHECK(at(rig.followed(), 2800, 2880));
    CHECK(!(rig.position() == rig.followed()));
    rig.stop_shake();
    CHECK(rig.trauma() == 0 && rig.position() == rig.followed());
}
