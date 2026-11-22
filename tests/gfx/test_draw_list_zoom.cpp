// Zoom in the 2D pass (spec 018 FR-9, decision D33): camera-relative sprites, glyphs, rectangles and lines
// land at (world − position) × zoom with sizes and pivots × zoom; outlines and lines stay one pixel thick;
// ignore_camera draws and the debug pass are never zoomed.

#include "gfx_fixture.hpp"
#include "nnh/debug.hpp"
#include "nnh/platform/video.hpp"

#include <cstring>

using nnh::Screen;
using nnh::Vec2;
using Item = nnh::platform::DrawItem2D;
using Kind = Item::Kind;
namespace gfx = nnh::gfx;
namespace fake = nnh::platform_fake;

namespace {

struct Zoomed {
    nnh::Texture hero; // 64 × 32
    Zoomed() {
        gfx_test::restart();
        hero = gfx_test::load("hero");
    }
    ~Zoomed() {
        hero.reset();
        nnh::shutdown();
    }
};

const Item& item(int i) {
    return fake::state().last_frame_items[i];
}
int items() {
    return fake::state().last_frame_item_count;
}

} // namespace

NNH_TEST(zoom_2_doubles_a_sprites_position_size_and_pivot) {
    Zoomed z;
    CHECK(nnh::frame());
    gfx::set_camera(Screen::Top, {{100, 50}, 2});
    gfx::draw(Screen::Top, z.hero, {110.25f, 60.5f},
              {.region = {0, 0, 16, 8}, .pivot = {4, 2}, .rotation = 0.5f, .scale = {-1.5f, 2}});
    CHECK(nnh::frame());
    CHECK(items() == 1);
    const Item& s = item(0);
    CHECK(s.x == 21 && s.y == 21); // snap(10.25 × 2) = snap(20.5), snap(10.5 × 2)
    CHECK(s.w == 48 && s.h == 32); // 16 × 1.5 × 2, 8 × 2 × 2
    CHECK(s.pivot_x == 12 && s.pivot_y == 8);
    CHECK(s.rotation == 0.5f && s.flip_x && !s.flip_y);
}

NNH_TEST(zoom_half_halves_positions_and_sizes) {
    Zoomed z;
    CHECK(nnh::frame());
    gfx::set_camera(Screen::Bottom, {{-20, 10}, 0.5f});
    gfx::draw(Screen::Bottom, z.hero, {80, 110}, {.pivot = {8, 4}});
    gfx::fill_rect(Screen::Bottom, {0, 10, 41, 21}, nnh::colors::red);
    CHECK(nnh::frame());
    CHECK(items() == 2);
    CHECK(item(0).x == 50 && item(0).y == 50 && item(0).w == 32 && item(0).h == 16);
    CHECK(item(0).pivot_x == 4 && item(0).pivot_y == 2);
    // Both corners snapped: x = snap(10) = 10, right = snap(61 × 0.5) = snap(30.5) = 31 → w 21; y 0, bottom
    // snap(10.5).
    CHECK(item(1).x == 10 && item(1).w == 21 && item(1).y == 0 && item(1).h == 11);
}

NNH_TEST(integer_zooms_put_shapes_on_exact_multiples) {
    Zoomed z;
    const float zooms[] = {2, 3, 4};
    for (float zoom : zooms) {
        CHECK(nnh::frame());
        gfx::set_camera(Screen::Top, {{10, 20}, zoom});
        gfx::fill_rect(Screen::Top, {13, 25, 7, 3}, nnh::colors::red);
        gfx::draw_rect(Screen::Top, {14, 21, 2, 9}, nnh::colors::blue);
        gfx::draw_line(Screen::Top, {11, 22}, {17, 29}, nnh::colors::green);
        CHECK(nnh::frame());
        CHECK(items() == 3);
        CHECK(item(0).x == 3 * zoom && item(0).y == 5 * zoom && item(0).w == 7 * zoom &&
              item(0).h == 3 * zoom);
        CHECK(item(1).kind == Kind::RectOutline && item(1).x == 4 * zoom && item(1).w == 2 * zoom);
        CHECK(item(2).kind == Kind::Line && item(2).x == 1 * zoom && item(2).y == 2 * zoom);
        CHECK(item(2).w == 7 * zoom && item(2).h == 9 * zoom); // the line's end point
    }
}

NNH_TEST(text_glyphs_scale_like_sprites) {
    Zoomed z;
    CHECK(nnh::frame());
    gfx::set_camera(Screen::Top, {{0, 0}, 2});
    gfx::draw_text(Screen::Top, {10, 20}, "AB", {.scale = 2});
    CHECK(nnh::frame());
    CHECK(items() == 2);
    CHECK(item(0).x == 20 && item(0).y == 40 && item(0).w == 32 && item(0).h == 32); // 8 × 2 × 2
    CHECK(item(1).x == 20 + 32);
}

NNH_TEST(ignore_camera_draws_are_not_zoomed) {
    Zoomed z;
    CHECK(nnh::frame());
    gfx::set_camera(Screen::Top, {{100, 100}, 3});
    gfx::draw(Screen::Top, z.hero, {10.5f, 20}, {.ignore_camera = true});
    gfx::fill_rect(Screen::Top, {1, 2, 30, 40}, nnh::colors::red, {.ignore_camera = true});
    gfx::draw_text(Screen::Top, {5, 6}, "x", {.ignore_camera = true});
    CHECK(nnh::frame());
    CHECK(items() == 3);
    CHECK(item(0).x == 11 && item(0).y == 20 && item(0).w == 64 && item(0).h == 32);
    CHECK(item(1).x == 1 && item(1).y == 2 && item(1).w == 30 && item(1).h == 40);
    CHECK(item(2).x == 5 && item(2).y == 6 && item(2).w == 8 && item(2).h == 8);
}

NNH_TEST(the_debug_pass_is_never_zoomed) {
    if (!nnh::build::kDevToolsEnabled) return; // no debug text in Release
    Zoomed z;
    CHECK(nnh::frame());
    gfx::set_camera(Screen::Top, {{100, 100}, 2});
    nnh::debug::print(Screen::Top, 0, 0, "D");
    CHECK(nnh::frame());
    CHECK(items() >= 1);
    const Item& last = item(items() - 1);
    CHECK(last.w == 8 && last.h == 8 && last.x < 16 && last.y < 16);
}
