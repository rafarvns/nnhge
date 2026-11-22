// Screen modes (spec 018 FR-10): single view on either screen, one tall world over both (bottom view centred
// under the top one, 40 px inset, a gap), and two views as two rigs.

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraLayout;
using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Screen;
using nnh::Vec2;
namespace cam = nnh::detail::cam;

namespace {

CameraRig* g_rig = nullptr;
void ask_undriven_screen() {
    (void)g_rig->camera(Screen::Bottom);
}

} // namespace

NNH_TEST(a_single_view_drives_only_its_screen) {
    CameraRig top;
    CHECK(top.drives(Screen::Top) && !top.drives(Screen::Bottom));
    CameraRig bottom({.screen = Screen::Bottom});
    CHECK(bottom.drives(Screen::Bottom) && !bottom.drives(Screen::Top));
    bottom.snap(Vec2{160, 120});
    CHECK(at(bottom.camera(Screen::Bottom).position, 0, 0));
}

NNH_TEST(asking_a_single_view_for_the_other_screen_asserts_and_answers_for_its_own) {
    CameraRig rig;
    rig.snap(Vec2{1000, 500});
    g_rig = &rig;
    CHECK(cam_test::asserts_or_is_ignored(ask_undriven_screen));
    if (!nnh::build::kAssertsEnabled) CHECK(at(rig.camera(Screen::Bottom).position, 800, 380));
}

NNH_TEST(tall_world_drives_both_screens_with_the_combined_view) {
    const CameraSettings s{.layout = CameraLayout::TallWorld, .gap = 16};
    CHECK(cam::view_size(s) == (Vec2{400, 496}));
    CameraRig rig(s);
    CHECK(rig.drives(Screen::Top) && rig.drives(Screen::Bottom));
    rig.snap(Vec2{1000, 1000}); // centre of the combined view: (200, 248)
    CHECK(at(rig.camera(Screen::Top).position, 800, 752));
    CHECK(at(rig.camera(Screen::Bottom).position, 840, 752 + 240 + 16));
}

NNH_TEST(tall_world_bottom_view_at_zoom_2_is_still_centred_under_the_top) {
    CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = 2, .gap = 8});
    CHECK(cam::view_size(rig.settings()) == (Vec2{200, 248}));
    rig.snap(Vec2{1000, 1000});
    const Vec2 top = rig.camera(Screen::Top).position;
    CHECK(at(top, 900, 876));
    CHECK(at(rig.camera(Screen::Bottom).position, 920, 876 + 120 + 8)); // inset 40 / 2, height 240 / 2
    // The bottom view's centre is under the top view's centre.
    CHECK(rig.view(Screen::Bottom).x + rig.view(Screen::Bottom).w / 2 == top.x + 100);
}

NNH_TEST(tall_world_limits_use_the_combined_height) {
    CameraRig rig({.layout = CameraLayout::TallWorld, .gap = 20});
    rig.set_limits({0, 0, 1000, 1000});
    rig.update(Vec2{5000, 5000});
    CHECK(at(rig.position(), 600, 1000 - 500));
    CHECK(rig.view(Screen::Bottom).y + rig.view(Screen::Bottom).h ==
          1000); // the bottom view ends at the limit
}

NNH_TEST(tall_world_dead_zone_is_around_the_combined_centre) {
    CameraRig rig({.layout = CameraLayout::TallWorld, .dead_zone = CameraSettings::centered_zone(40, 100)});
    rig.snap(Vec2{1000, 1000});
    rig.update(Vec2{1000, 1049}); // inside
    CHECK(at(rig.position(), 800, 760));
    rig.update(Vec2{1000, 1060}); // 10 past
    CHECK(at(rig.position(), 800, 770));
}

NNH_TEST(a_point_crossing_from_the_top_to_the_bottom_screen_moves_continuously) {
    const float zooms[] = {1, 2, 0.5f};
    for (float z : zooms) {
        CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = z, .gap = 24});
        rig.snap(Vec2{500, 500});
        const Vec2 top = rig.camera(Screen::Top).position;
        // A sprite on the top screen's last pixel row, then just below the hidden gap.
        const Vec2 last_top_row{top.x + 200 / z, top.y + 239 / z};
        CHECK(rig.to_screen(Screen::Top, last_top_row).y == 239);
        const Vec2 first_bottom_row{top.x + 200 / z, top.y + 240 / z + 24};
        const Vec2 on_bottom = rig.to_screen(Screen::Bottom, first_bottom_row);
        CHECK(on_bottom.y == 0);
        CHECK(on_bottom.x == rig.to_screen(Screen::Top, first_bottom_row).x - 40); // same column, 40 px inset
    }
}

NNH_TEST(a_layout_change_keeps_the_primary_top_left) {
    CameraRig rig;
    rig.snap(Vec2{1000, 500});
    rig.set_layout(CameraLayout::TallWorld);
    CHECK(at(rig.position(), 800, 380) && rig.drives(Screen::Bottom));
    rig.update(Vec2{1000, 500}); // the next update re-centres on the combined view
    CHECK(at(rig.position(), 800, 260));
    rig.set_layout(CameraLayout::Single, Screen::Bottom);
    CHECK(rig.drives(Screen::Bottom) && !rig.drives(Screen::Top) && at(rig.position(), 800, 260));
}

NNH_TEST(two_views_are_two_rigs_on_their_own_screens) {
    CameraRig game;
    CameraRig map({.screen = Screen::Bottom, .zoom = 0.5f});
    game.snap(Vec2{1000, 500});
    map.snap(Vec2{2000, 1000});
    CHECK(at(game.camera(Screen::Top).position, 800, 380));
    CHECK(at(map.camera(Screen::Bottom).position, 1680, 760)); // 640 × 480 world pixels
}
