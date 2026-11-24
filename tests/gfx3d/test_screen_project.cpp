// Screen and world (spec 026 FR-13/FR-27/FR-28/FR-30, T4.8–T4.11): to_screen and screen_ray round trips, flags,
// orthographic cameras; stats and the overlay line; the debug draw's lines; no allocation over 100 frames of 256 draws;
// no event published.

#include <cmath>
#include <cstring>

#include "alloc_counter.hpp"
#include "nnh/memory.hpp"
#include "gfx3d/gfx3d_state.hpp"
#include "gfx3d_fixture.hpp"

using namespace gfx3d_test;
using nnh::Screen;
using nnh::Vec2;
using nnh::Vec3;

namespace {
nnh::Mesh g_cube;
}

NNH_TEST(gfx3d_to_screen_centre_edges_and_flags) {
    const nnh::Camera3D cam; // at (0, 0, 5), looking at the origin, 60°
    const nnh::ScreenPoint c = nnh::gfx3d::to_screen(cam, {0, 0, 0});
    CHECK(c.pixel == (Vec2{200, 120}) && c.depth == 5 && c.in_front && c.in_view);
    const float top = 5 * 0.57735027f; // tan 30° × 5: the top edge
    const nnh::ScreenPoint t = nnh::gfx3d::to_screen(cam, {0, top, 0});
    CHECK(std::fabs(t.pixel.y) < 1e-3f && std::fabs(t.pixel.x - 200) < 1e-3f);
    const nnh::ScreenPoint behind = nnh::gfx3d::to_screen(cam, {0, 0, 6});
    CHECK(!behind.in_front && !behind.in_view && behind.depth == -1);
    CHECK(!nnh::gfx3d::to_screen(cam, {0, 5, 0}).in_view && nnh::gfx3d::to_screen(cam, {0, 5, 0}).in_front);
}

NNH_TEST(gfx3d_screen_ray_round_trips_with_to_screen) {
    nnh::Camera3D cam;
    cam.position = {3, 4, 8};
    nnh::gfx3d::point_at(cam, {0, 1, 0});
    for (int i = 0; i < 50; ++i) {
        const Vec2 px{static_cast<float>(i * 8), static_cast<float>((i * 37) % 240)};
        const nnh::Ray3 r = nnh::gfx3d::screen_ray(cam, px);
        CHECK(std::fabs(nnh::length(r.direction) - 1) < 1e-6f);
        const nnh::ScreenPoint back = nnh::gfx3d::to_screen(cam, r.origin + r.direction * 7.0f);
        CHECK(std::fabs(back.pixel.x - px.x) < 2e-3f && std::fabs(back.pixel.y - px.y) < 2e-3f);
    }
    cam.projection = nnh::Projection::Orthographic;
    const nnh::Ray3 o = nnh::gfx3d::screen_ray(cam, {50, 30});
    const nnh::ScreenPoint ob = nnh::gfx3d::to_screen(cam, o.origin + o.direction * 3.0f);
    CHECK(std::fabs(ob.pixel.x - 50) < 2e-3f && std::fabs(ob.pixel.y - 30) < 2e-3f);
    CHECK(nnh::gfx3d::screen_ray(cam, {0, 0}).direction == nnh::gfx3d::screen_ray(cam, {399, 239}).direction);
}

NNH_TEST(gfx3d_to_screen_by_screen_uses_the_set_camera) {
    restart();
    nnh::Camera3D cam;
    cam.position = {0, 0, 10};
    nnh::gfx3d::set_camera(Screen::Top, cam);
    CHECK(nnh::gfx3d::to_screen(Screen::Top, {0, 0, 0}).depth == 10);
    nnh::shutdown();
}

NNH_TEST(gfx3d_stats_overlay_line_and_debug_lines) {
    restart();
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const nnh::Stats3D s = nnh::gfx3d::stats();
    CHECK(s.draws == 1 && s.meshes == 1 && s.mesh_bytes > 0 && s.peak_draws == 1 && s.triangles[0] == 12);
    char line[96];
    CHECK(nnh::detail::gfx3d_overlay_line(line, sizeof line) && std::strstr(line, "3d 1/64 draws") != nullptr);
    nnh::gfx3d::set_debug_draw(true);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    // 12 box edges + 3 axes as 2D lines on layer 15 (Debug/Development only).
    const int lines = fake::state().last_frame_item_count;
    CHECK(lines == (nnh::build::kDevToolsEnabled ? 15 : 0));
    nnh::gfx3d::set_debug_draw(false);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_frames_allocate_nothing) {
    restart(256);
    g_cube = cube();
    next_frame();
    const int live = nnh::mem::live_allocations();
    nnh::test::alloc_counter::reset();
    nnh::test::alloc_counter::enable(true);
    for (int f = 0; f < 100; ++f) {
        for (int i = 0; i < 256; ++i) {
            const nnh::Material3D glass{.alpha = (i % 3 == 0) ? nnh::AlphaMode::Blend : nnh::AlphaMode::Opaque};
            nnh::gfx3d::draw(Screen::Top, g_cube, at({static_cast<float>(i % 16) - 8, 0, -static_cast<float>(i / 16)}),
                             {.material = &glass});
        }
        next_frame();
    }
    nnh::test::alloc_counter::enable(false);
    CHECK(nnh::test::alloc_counter::count() == 0 && nnh::mem::live_allocations() == live);
    CHECK(nnh::gfx3d::stats().draws == 256);
    g_cube.reset();
    nnh::shutdown();
}
