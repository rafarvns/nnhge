// An end-to-end frame through core and the fake platform's rasteriser (spec 026 FR-25, T5.6): an unlit cube in front
// of the camera covers the centre with its colour, the corners keep the clear colour; a lit one is brighter facing
// the light; textures from the fake's gradient images reach the pixels.

#include "gfx3d_fixture.hpp"

using namespace gfx3d_test;
using nnh::Screen;

namespace {
nnh::Mesh g_cube;
}

NNH_TEST(gfx3d_fake_pixels_show_an_unlit_cube) {
    restart();
    nnh::gfx::set_clear_color(Screen::Top, {10, 20, 30, 255});
    fake::enable_pixels_3d(true);
    g_cube = cube(2, {.base_color = {200, 100, 50, 255}, .lit = false});
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const nnh::Color c = fake::pixel_3d(200, 120), corner = fake::pixel_3d(2, 2);
    CHECK(c.r == 200 && c.g == 100 && c.b == 50);
    CHECK(corner.r == 10 && corner.g == 20 && corner.b == 30);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_fake_pixels_light_the_side_facing_the_light) {
    restart();
    fake::enable_pixels_3d(true);
    g_cube = cube(2, {.base_color = {200, 200, 200, 255}});
    next_frame();
    nnh::Camera3D cam;
    cam.position = {3, 2, 5};
    nnh::gfx3d::point_at(cam, {});
    nnh::gfx3d::set_camera(Screen::Top, cam);
    nnh::Lighting l;
    l.ambient = {20, 20, 20, 255};
    l.lights[0] = {.vector = {1, 0, 0}}; // from +x
    nnh::gfx3d::set_lighting(Screen::Top, l);
    nnh::gfx3d::draw(Screen::Top, g_cube, at({}));
    next_frame();
    const nnh::ScreenPoint right = nnh::gfx3d::to_screen(cam, {1, 0, 0.2f});  // on the +x face
    const nnh::ScreenPoint front = nnh::gfx3d::to_screen(cam, {0.2f, 0, 1}); // on the +z face
    const nnh::Color r = fake::pixel_3d(int(right.pixel.x), int(right.pixel.y));
    const nnh::Color f = fake::pixel_3d(int(front.pixel.x), int(front.pixel.y));
    std::printf("    lit %d unlit %d at (%g, %g) (%g, %g)\n", r.r, f.r, right.pixel.x, right.pixel.y, front.pixel.x, front.pixel.y);
    CHECK(r.r == 200 && f.r == 16); // lit: 200 × min(0.08 + 1, 1); unlit side: ambient only (200 × 20/255 = 16)
    g_cube.reset();
    nnh::shutdown();
}
