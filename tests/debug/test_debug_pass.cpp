// Debug text drawn with the built-in font in the debug pass, after the 2D passes (spec 005 FR-6).

#include <cstring>

#include "../gfx/gfx_fixture.hpp"
#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "gfx/gfx_state.hpp"
#include "nnh/debug.hpp"
#include "text/font_atlas.hpp"
#include "text/glyphs.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::restart;
using nnh::Screen;
using Kind = nnh::platform::DrawItem2D::Kind;

namespace {

const nnh::platform::DrawItem2D& item(int i) { return fake::state().last_frame_items[i]; }

} // namespace

NNH_TEST(debug_text_is_drawn_with_the_font_after_the_game) {
    restart();
    CHECK(nnh::frame());
    nnh::debug::print(Screen::Top, 3, 4, "Hi", nnh::colors::red); // printed before the game draws...
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 10, 10}, nnh::colors::blue);
    CHECK(nnh::frame());
    CHECK(std::strcmp(fake::state().last_frame_calls, "TT") == 0); // ...but in its own pass after it
    CHECK(fake::state().last_frame_item_count == 3);
    CHECK(item(0).kind == Kind::FillRect);
    const nnh::Region h = nnh::detail::glyph_region(nnh::detail::glyph_index(U'H'));
    CHECK(item(1).kind == Kind::Sprite && item(1).region.x == h.x && item(1).region.y == h.y);
    CHECK(item(1).x == 3 && item(1).y == 4 && item(1).w == 8 && item(1).h == 8);
    CHECK(item(1).color.r == 255 && item(1).color.b == 0);
    CHECK(item(2).x == 11);
    nnh::shutdown();
}

NNH_TEST(debug_text_uses_the_same_font_texture_as_draw_text) {
    restart();
    CHECK(nnh::frame());
    nnh::gfx::draw_text(Screen::Bottom, {0, 0}, "a");
    nnh::debug::print(Screen::Bottom, 0, 20, "a");
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 2);
    CHECK(item(0).texture == item(1).texture && item(0).region.x == item(1).region.x);
    nnh::shutdown();
}

NNH_TEST(debug_text_on_one_screen_only_submits_that_screen) {
    restart();
    CHECK(nnh::frame());
    nnh::debug::print(Screen::Bottom, 0, 0, "x");
    CHECK(nnh::frame());
    CHECK(std::strcmp(fake::state().last_frame_calls, "B") == 0);
    nnh::shutdown();
}

NNH_TEST(debug_line_height_is_the_font_line_height_on_every_system) {
    restart();
    CHECK(nnh::debug::line_height() == nnh::kFontLineHeight);
    nnh::shutdown();
}

NNH_TEST(the_last_frames_debug_text_stays_readable_for_tests) {
    restart();
    CHECK(nnh::frame());
    nnh::debug::printf(Screen::Top, 1, 2, "score %d", 42);
    CHECK(nnh::frame());
    const nnh::detail::DebugList& last = nnh::detail::debug_list_last_frame();
    CHECK(last.count() == 1 && std::strcmp(last.text(0), "score 42") == 0);
    CHECK(last.at(0).x == 1 && last.at(0).y == 2);
    CHECK(nnh::frame());
    CHECK(nnh::detail::debug_list_last_frame().count() == 0);
    nnh::shutdown();
}

NNH_TEST(debug_text_has_its_own_capacity_apart_from_max_draws) {
    restart({.gfx = {.max_draws = 16}});
    CHECK(nnh::frame());
    nnh::debug::print(Screen::Top, 0, 0, "This line has more characters than max_draws allows.");
    CHECK(nnh::frame()); // no assert: the debug pass doesn't use the game's draw list
    CHECK(fake::state().last_frame_item_count > 16);
    nnh::shutdown();
}

NNH_TEST(debug_text_beyond_the_debug_capacity_is_dropped) {
    restart();
    static char line[201];
    for (int i = 0; i < 200; ++i) line[i] = 'x';
    CHECK(nnh::frame());
    for (int i = 0; i < 30; ++i) nnh::debug::print(Screen::Top, 0, 0, line); // 6000 characters
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == nnh::detail::kDebugPassCapacity);
    nnh::shutdown();
}
