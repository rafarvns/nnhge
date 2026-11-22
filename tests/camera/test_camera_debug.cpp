// debug_draw() (spec 018 FR-14): dead zone (yellow), limits (red) and the target (green cross) on the driven
// screen(s), layer 15, in screen pixels; nothing outside a frame; nothing at all in Release.

#include "camera_fixture.hpp"
#include "nnh/platform/video.hpp"

using nnh::CameraLayout;
using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Screen;
using nnh::Vec2;
namespace fake = nnh::platform_fake;
using Kind = nnh::platform::DrawItem2D::Kind;

namespace {

bool same_color(nnh::Color a, nnh::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/// Items of the last frame with this colour (and on layer 15).
int count(nnh::Color color, Kind kind) {
    int n = 0;
    for (int i = 0; i < fake::state().last_frame_item_count; ++i) {
        const nnh::platform::DrawItem2D& it = fake::state().last_frame_items[i];
        if (it.kind == kind && same_color(it.color, color) && it.layer == 15) ++n;
    }
    return n;
}

const nnh::platform::DrawItem2D* first(nnh::Color color) {
    for (int i = 0; i < fake::state().last_frame_item_count; ++i) {
        if (same_color(fake::state().last_frame_items[i].color, color)) return &fake::state().last_frame_items[i];
    }
    return nullptr;
}

} // namespace

NNH_TEST(debug_draw_shows_the_dead_zone_limits_and_target_on_the_top_layer) {
    cam_test::restart();
    CameraRig rig({.dead_zone = CameraSettings::centered_zone(64, 48)});
    rig.set_limits({700, 300, 2000, 1000});
    rig.snap(Vec2{1000, 500});
    CHECK(nnh::frame());
    rig.apply();
    rig.debug_draw();
    CHECK(nnh::frame());
    if (!nnh::build::kDevToolsEnabled) {
        CHECK(fake::state().last_frame_item_count == 0);
        nnh::shutdown();
        return;
    }
    CHECK(count(nnh::colors::yellow, Kind::RectOutline) == 1);
    CHECK(count(nnh::colors::red, Kind::RectOutline) == 1);
    CHECK(count(nnh::colors::green, Kind::Line) == 2);
    const auto* zone = first(nnh::colors::yellow);
    CHECK(zone != nullptr && zone->x == 200 - 32 && zone->y == 120 - 24 && zone->w == 64 && zone->h == 48);
    const auto* limits = first(nnh::colors::red);
    CHECK(limits != nullptr && limits->x == -100 && limits->y == -80); // in screen pixels, ignoring the camera
    nnh::shutdown();
}

NNH_TEST(debug_draw_in_tall_world_draws_on_both_screens) {
    cam_test::restart();
    CameraRig rig({.layout = CameraLayout::TallWorld, .dead_zone = CameraSettings::centered_zone(40, 40)});
    rig.snap(Vec2{1000, 1000});
    CHECK(nnh::frame());
    rig.debug_draw();
    CHECK(nnh::frame());
    if (nnh::build::kDevToolsEnabled) {
        CHECK(count(nnh::colors::yellow, Kind::RectOutline) == 2); // the zone straddles the screens
        CHECK(count(nnh::colors::green, Kind::Line) == 4);
    }
    nnh::shutdown();
}

NNH_TEST(debug_draw_outside_a_frame_draws_nothing) {
    cam_test::restart();
    CameraRig rig({.dead_zone = CameraSettings::centered_zone(40, 40)});
    rig.debug_draw(); // before the first frame: ignored
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}
