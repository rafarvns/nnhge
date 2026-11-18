// Draw calls, shapes and cameras reaching the platform at the end of the frame (nnh/gfx.hpp, spec 004
// FR-4, FR-5, FR-6, FR-7, FR-8, FR-12).

#include <cstring>

#include "gfx_fixture.hpp"
#include "nnh/debug.hpp"
#include "nnh/build.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::load;
using gfx_test::restart;
using Kind = nnh::platform::DrawItem2D::Kind;
using nnh::Screen;

namespace {

const nnh::platform::DrawItem2D& item(int i) { return fake::state().last_frame_items[i]; }
const char* calls() { return fake::state().last_frame_calls; }

void overfill() {
    for (int i = 0; i < 100; ++i) nnh::gfx::fill_rect(Screen::Top, {0, 0, 1, 1}, nnh::colors::red);
}

} // namespace

NNH_TEST(draws_outside_a_frame_are_ignored) {
    restart();
    nnh::Texture hero = load("hero");
    nnh::gfx::draw(Screen::Top, hero, {1, 2});
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 4, 4}, nnh::colors::red);
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}

NNH_TEST(a_sprite_reaches_the_platform_with_its_options) {
    restart();
    nnh::Texture hero = load("hero");
    CHECK(nnh::frame());
    nnh::gfx::draw(Screen::Top, hero, {10, 20},
                   {.region = {16, 0, 16, 16},
                    .pivot = {8, 8},
                    .rotation = 0.5f,
                    .scale = {2, 2},
                    .flip_x = true,
                    .tint = nnh::rgba(255, 0, 0, 200),
                    .opacity = 0.5f,
                    .layer = 3});
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 1);
    const auto& s = item(0);
    CHECK(s.kind == Kind::Sprite && s.texture != 0);
    CHECK(s.region.x == 16 && s.region.w == 16);
    CHECK(s.x == 10 && s.y == 20 && s.w == 32 && s.h == 32);
    CHECK(s.pivot_x == 16 && s.rotation == 0.5f && s.flip_x && !s.flip_y);
    CHECK(s.color.r == 255 && s.color.g == 0 && s.color.a == 100); // opacity halves the tint's alpha
    CHECK(s.layer == 3);
    nnh::shutdown();
}

NNH_TEST(drawing_an_empty_texture_does_nothing) {
    restart();
    nnh::Texture none;
    CHECK(nnh::frame());
    nnh::gfx::draw(Screen::Top, none, {0, 0});
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 0);
    nnh::shutdown();
}

NNH_TEST(shapes_reach_the_platform) {
    restart();
    CHECK(nnh::frame());
    nnh::gfx::fill_rect(Screen::Bottom, {1, 2, 30, 40}, nnh::colors::red);
    nnh::gfx::draw_rect(Screen::Bottom, {5, 6, 7, 8}, nnh::colors::green);
    nnh::gfx::draw_line(Screen::Bottom, {0, 0}, {100, 50}, nnh::colors::blue);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 3);
    CHECK(item(0).kind == Kind::FillRect && item(0).x == 1 && item(0).w == 30 && item(0).color.r == 255);
    CHECK(item(1).kind == Kind::RectOutline && item(1).h == 8);
    CHECK(item(2).kind == Kind::Line && item(2).w == 100 && item(2).h == 50);
    nnh::shutdown();
}

NNH_TEST(each_screen_has_its_own_camera) {
    restart();
    nnh::gfx::set_camera(Screen::Top, {{100, 10}});
    CHECK(nnh::gfx::camera(Screen::Top).position == (nnh::Vec2{100, 10}));
    CHECK(nnh::gfx::camera(Screen::Bottom).position == (nnh::Vec2{0, 0}));
    CHECK(nnh::frame());
    nnh::gfx::fill_rect(Screen::Top, {150, 50, 1, 1}, nnh::colors::red);
    nnh::gfx::fill_rect(Screen::Top, {150, 50, 1, 1}, nnh::colors::red, {.ignore_camera = true});
    nnh::gfx::fill_rect(Screen::Bottom, {150, 50, 1, 1}, nnh::colors::red);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 3);
    CHECK(item(0).x == 50 && item(0).y == 40);  // top, camera applied
    CHECK(item(1).x == 150 && item(1).y == 50); // top, HUD
    CHECK(item(2).x == 150);                    // bottom: its own camera
    nnh::shutdown();
}

NNH_TEST(cameras_are_reset_by_init) {
    restart();
    nnh::gfx::set_camera(Screen::Top, {{5, 5}});
    restart();
    CHECK(nnh::gfx::camera(Screen::Top).position == (nnh::Vec2{0, 0}));
    nnh::shutdown();
}

NNH_TEST(the_frame_submits_top_then_bottom_then_debug_text_on_top) {
    restart();
    CHECK(nnh::frame());
    nnh::debug::print(Screen::Top, 0, 0, "hud"); // printed first...
    nnh::gfx::fill_rect(Screen::Bottom, {0, 0, 1, 1}, nnh::colors::red);
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 1, 1}, nnh::colors::red);
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "TBT") == 0); // ...but drawn after the 2D passes, in the debug pass
    nnh::shutdown();
}

NNH_TEST(draws_on_higher_layers_come_later) {
    restart();
    CHECK(nnh::frame());
    nnh::gfx::fill_rect(Screen::Top, {1, 0, 1, 1}, nnh::colors::red, {.layer = 9});
    nnh::gfx::fill_rect(Screen::Top, {2, 0, 1, 1}, nnh::colors::red);
    CHECK(nnh::frame());
    CHECK(item(0).x == 2 && item(1).x == 1);
    nnh::shutdown();
}

NNH_TEST(stats_report_the_last_frame_and_the_capacities) {
    restart({.gfx = {.max_draws = 64}});
    CHECK(nnh::frame());
    for (int i = 0; i < 5; ++i) nnh::gfx::fill_rect(Screen::Top, {0, 0, 1, 1}, nnh::colors::red);
    CHECK(nnh::frame());
    const nnh::gfx::Stats s = nnh::gfx::stats();
    CHECK(s.draws_last_frame == 5 && s.max_draws == 64 && s.dropped_draws == 0);
    nnh::shutdown();
}

NNH_TEST(too_many_draws_assert_or_are_dropped_and_logged) {
    restart({.gfx = {.max_draws = 16}});
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(overfill);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (!nnh::build::kAssertsEnabled) {
        CHECK(nnh::frame());
        CHECK(fake::state().last_frame_item_count == 16);
        CHECK(nnh::gfx::stats().dropped_draws == 100 - 16);
        CHECK(std::strstr(fake::state().last_console_line, "draws") != nullptr);
    }
    nnh::shutdown();
}
