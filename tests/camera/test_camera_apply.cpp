// apply() (spec 018 FR-13): writes the driven screen(s)' Camera2D, leaves the other screen alone; conversions match
// what the renderer then draws (FR-11).

#include "../gfx/gfx_fixture.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraLayout;
using nnh::CameraRig;
using nnh::Screen;
using nnh::Vec2;
namespace gfx = nnh::gfx;
namespace fake = nnh::platform_fake;

NNH_TEST(apply_writes_only_the_driven_screen) {
    cam_test::restart();
    gfx::set_camera(Screen::Bottom, {{7, 9}});
    CameraRig rig;
    rig.snap(Vec2{1000, 500});
    rig.apply();
    CHECK(at(gfx::camera(Screen::Top).position, 800, 380));
    CHECK(at(gfx::camera(Screen::Bottom).position, 7, 9));
    CameraRig bottom({.screen = Screen::Bottom});
    bottom.snap(Vec2{1000, 500});
    bottom.apply();
    CHECK(at(gfx::camera(Screen::Bottom).position, 840, 380));
    CHECK(at(gfx::camera(Screen::Top).position, 800, 380));
    nnh::shutdown();
}

NNH_TEST(apply_in_tall_world_writes_both_screens) {
    cam_test::restart();
    CameraRig rig({.layout = CameraLayout::TallWorld, .gap = 10});
    rig.snap(Vec2{1000, 1000});
    rig.apply();
    CHECK(at(gfx::camera(Screen::Top).position, 800, 755));
    CHECK(at(gfx::camera(Screen::Bottom).position, 840, 755 + 250));
    nnh::shutdown();
}

NNH_TEST(apply_writes_the_zoom_too) {
    cam_test::restart();
    CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = 2});
    rig.snap(Vec2{1000, 1000});
    rig.apply();
    CHECK(gfx::camera(Screen::Top).zoom == 2 && gfx::camera(Screen::Bottom).zoom == 2);
    CHECK(at(gfx::camera(Screen::Top).position, 900, 880));
    CHECK(rig.camera(Screen::Bottom).zoom == 2);
    nnh::shutdown();
}

NNH_TEST(a_sprite_drawn_at_to_world_lands_under_that_screen_point_at_zoom_3) {
    gfx_test::restart();
    nnh::Texture tile = gfx_test::load("tile");
    CameraRig rig({.layout = CameraLayout::TallWorld, .zoom = 3, .gap = 6});
    rig.snap(Vec2{1234.25f, 987.75f});
    CHECK(nnh::frame());
    rig.apply();
    gfx::draw(Screen::Bottom, tile, rig.to_world(Screen::Bottom, {100, 50}));
    CHECK(nnh::frame());
    const nnh::platform::DrawItem2D& it = fake::state().last_frame_items[0];
    CHECK(it.x >= 99 && it.x <= 101 && it.y >= 49 && it.y <= 51); // within the renderer's whole-pixel rounding
    CHECK(it.w == 48);
    tile.reset();
    nnh::shutdown();
}

NNH_TEST(apply_writes_the_pixel_snapped_camera) {
    cam_test::restart();
    CameraRig rig({.pixel_snap = true});
    rig.snap(Vec2{1000.3f, 500.7f});
    rig.apply();
    CHECK(at(gfx::camera(Screen::Top).position, 800, 381));
    CHECK(!(rig.position() == gfx::camera(Screen::Top).position));
    nnh::shutdown();
}

NNH_TEST(a_sprite_drawn_at_to_world_lands_under_that_screen_point) {
    gfx_test::restart();
    nnh::Texture tile = gfx_test::load("tile");
    CameraRig rig({.layout = CameraLayout::TallWorld, .gap = 6});
    rig.snap(Vec2{1234.25f, 987.75f});
    CHECK(nnh::frame());
    rig.apply();
    const Vec2 touch{100, 50}; // on the bottom screen
    gfx::draw(Screen::Bottom, tile, rig.to_world(Screen::Bottom, touch));
    gfx::draw(Screen::Top, tile, rig.to_world(Screen::Top, {399, 0}));
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 2);
    const nnh::platform::DrawItem2D& top = fake::state().last_frame_items[0];
    const nnh::platform::DrawItem2D& bottom = fake::state().last_frame_items[1];
    CHECK(top.x == 399 && top.y == 0);
    CHECK(bottom.x == 100 && bottom.y == 50);
    tile.reset();
    nnh::shutdown();
}
