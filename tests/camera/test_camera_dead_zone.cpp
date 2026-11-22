// The dead zone (spec 018 FR-3): screen pixels around the view's centre; inside it the target moves freely,
// leaving it drags the view by exactly the overshoot; larger than the view → clamped and logged once.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Vec2;

namespace {

/// Centred 64 × 48 zone, snapped on (1000, 500): view (800, 380), zone x [968, 1032), y [476, 524).
CameraRig centred_rig() {
    CameraRig rig({.dead_zone = CameraSettings::centered_zone(64, 48)});
    rig.snap(Vec2{1000, 500});
    return rig;
}

} // namespace

NNH_TEST(target_inside_the_dead_zone_leaves_the_camera_still) {
    CameraRig rig = centred_rig();
    const Vec2 inside[] = {{1000, 500}, {970, 480}, {1031, 523}, {968, 476}, {1032, 524}};
    for (Vec2 t : inside) {
        rig.update(t);
        CHECK(at(rig.position(), 800, 380));
    }
}

NNH_TEST(leaving_each_edge_moves_the_camera_by_exactly_the_overshoot) {
    CameraRig rig = centred_rig();
    rig.update(Vec2{1040, 500}); // 8 past the right edge (1032)
    CHECK(at(rig.position(), 808, 380));
    rig.update(Vec2{1040, 500}); // now inside
    CHECK(at(rig.position(), 808, 380));
    rig.update(Vec2{970, 500}); // left edge is now 976: 6 past it
    CHECK(at(rig.position(), 802, 380));
    rig.update(Vec2{970, 534}); // bottom edge 524: 10 past
    CHECK(at(rig.position(), 802, 390));
    rig.update(Vec2{970, 476}); // top edge 486: 10 past
    CHECK(at(rig.position(), 802, 380));
}

NNH_TEST(an_asymmetric_zone_holds_the_target_off_centre) {
    // Zone x [-10, +50) right of the centre: the target can run 50 px right but only 10 px left.
    CameraRig rig({.dead_zone = {-10, 0, 60, 0}});
    rig.snap(Vec2{1000, 500}); // snap centres: zone [990, 1050)
    rig.update(Vec2{1050, 500});
    CHECK(rig.position().x == 800);
    rig.update(Vec2{1060, 500});
    CHECK(rig.position().x == 810);
    rig.update(Vec2{980, 500}); // left edge 1000: 20 past
    CHECK(rig.position().x == 790);
}

NNH_TEST(a_zero_width_zone_away_from_the_centre_holds_the_target_at_that_point) {
    CameraRig rig({.dead_zone = {40, -20, 0, 0}});
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.position(), 760, 400)); // the target sits 40 right, 20 above the view's centre
}

NNH_TEST(a_dead_zone_larger_than_the_view_is_clamped_to_it_and_logged_once) {
    cam_test::restart();
    CameraRig rig({.dead_zone = CameraSettings::centered_zone(1000, 1000)});
    rig.snap(Vec2{1000, 500});
    rig.update(Vec2{1300, 500}); // the zone is the whole view [800, 1200): 100 past it
    CHECK(rig.position().x == 900);
    if (nnh::build::kDevToolsEnabled) CHECK(cam_test::last_log_mentions("dead zone"));
    const int lines = nnh::platform_fake::state().console_lines;
    rig.update(Vec2{1400, 500});
    CHECK(rig.position().x == 1000);
    CHECK(nnh::platform_fake::state().console_lines == lines); // once per rig
    nnh::shutdown();
}

NNH_TEST(zone_axis_clamps_only_what_passes_the_view) {
    bool clamped = false;
    nnh::detail::cam::Zone z = nnh::detail::cam::zone_axis(-32, 64, 1, 200, clamped);
    CHECK(z.pos == -32 && z.size == 64 && !clamped);
    z = nnh::detail::cam::zone_axis(-300, 400, 1, 200, clamped);
    CHECK(z.pos == -200 && z.size == 300 && clamped);
    clamped = false;
    z = nnh::detail::cam::zone_axis(150, 100, 1, 200, clamped);
    CHECK(z.pos == 150 && z.size == 50 && clamped);
    clamped = false;
    z = nnh::detail::cam::zone_axis(250, 10, 1, 200, clamped); // starts past the view: pinned to its edge
    CHECK(z.pos == 200 && z.size == 0 && clamped);
}
