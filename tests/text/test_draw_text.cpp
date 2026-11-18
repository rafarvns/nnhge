// Drawing and measuring text with the built-in font (nnh/gfx.hpp, spec 005 FR-3, FR-4, FR-8).

#include "../gfx/gfx_fixture.hpp"
#include "text/font_atlas.hpp"
#include "text/glyphs.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::restart;
using nnh::Screen;
using nnh::detail::glyph_index;
using nnh::detail::glyph_region;
using Kind = nnh::platform::DrawItem2D::Kind;

namespace {

const nnh::platform::DrawItem2D& item(int i) { return fake::state().last_frame_items[i]; }
int items() { return fake::state().last_frame_item_count; }

bool region_of(const nnh::platform::DrawItem2D& it, char32_t c) {
    const nnh::Region r = glyph_region(glyph_index(c));
    return it.region.x == r.x && it.region.y == r.y && it.region.w == 8 && it.region.h == 8;
}

/// Draws in one frame and ends it, so the items reach the fake platform.
template <typename Fn>
void frame_with(Fn&& draw) {
    CHECK(nnh::frame());
    draw();
    CHECK(nnh::frame());
}

} // namespace

NNH_TEST(text_becomes_one_sprite_per_character_from_the_font) {
    restart();
    frame_with([] { nnh::gfx::draw_text(Screen::Top, {10, 20}, "Hi"); });
    CHECK(items() == 2);
    CHECK(item(0).kind == Kind::Sprite && item(0).texture != 0);
    CHECK(item(0).texture == item(1).texture); // one texture: the font
    CHECK(region_of(item(0), U'H') && region_of(item(1), U'i'));
    CHECK(item(0).x == 10 && item(0).y == 20 && item(0).w == 8 && item(0).h == 8);
    CHECK(item(1).x == 18 && item(1).y == 20);
    nnh::shutdown();
}

NNH_TEST(text_outside_a_frame_or_null_is_ignored) {
    restart();
    nnh::gfx::draw_text(Screen::Top, {0, 0}, "outside");
    frame_with([] { nnh::gfx::draw_text(Screen::Top, {0, 0}, nullptr); });
    CHECK(items() == 0);
    nnh::shutdown();
}

NNH_TEST(text_spaces_and_line_breaks_take_room_without_drawing) {
    restart();
    frame_with([] { nnh::gfx::draw_text(Screen::Bottom, {0, 0}, "a b\nc"); });
    CHECK(items() == 3);
    CHECK(item(1).x == 16 && item(1).y == 0);
    CHECK(item(2).x == 0 && item(2).y == nnh::kFontLineHeight);
    nnh::shutdown();
}

NNH_TEST(text_color_scale_and_layer_reach_every_character) {
    restart();
    frame_with([] {
        nnh::gfx::draw_text(Screen::Top, {0, 0}, "ab",
                            {.color = nnh::rgb(255, 200, 0), .scale = 2, .layer = 7});
    });
    CHECK(items() == 2);
    CHECK(item(0).w == 16 && item(0).h == 16 && item(1).x == 16);
    CHECK(item(1).color.r == 255 && item(1).color.g == 200 && item(1).color.b == 0);
    CHECK(item(0).layer == 7 && item(1).layer == 7);
    nnh::shutdown();
}

NNH_TEST(text_alignment_is_relative_to_the_position) {
    restart();
    frame_with([] {
        nnh::gfx::draw_text(Screen::Top, {100, 0}, "abcd", {.align = nnh::TextAlign::Center});
        nnh::gfx::draw_text(Screen::Top, {100, 20}, "ab", {.align = nnh::TextAlign::Right});
    });
    CHECK(items() == 6);
    CHECK(item(0).x == 84);
    CHECK(item(4).x == 84 && item(5).x == 92);
    nnh::shutdown();
}

NNH_TEST(text_shadow_draws_under_each_character) {
    restart();
    frame_with([] {
        nnh::gfx::draw_text(Screen::Top, {10, 10}, "A",
                            {.color = nnh::colors::white, .scale = 2, .shadow = true,
                             .shadow_color = nnh::rgba(0, 0, 0, 128)});
    });
    CHECK(items() == 2);
    CHECK(item(0).x == 12 && item(0).y == 12 && item(0).color.a == 128); // shadow first, offset by scale
    CHECK(item(1).x == 10 && item(1).y == 10 && item(1).color.r == 255);
    CHECK(region_of(item(0), U'A') && region_of(item(1), U'A'));
    nnh::shutdown();
}

NNH_TEST(text_follows_the_camera_unless_it_ignores_it) {
    restart();
    nnh::gfx::set_camera(Screen::Top, {{50, 0}});
    frame_with([] {
        nnh::gfx::draw_text(Screen::Top, {100, 0}, "x");
        nnh::gfx::draw_text(Screen::Top, {100, 0}, "y", {.ignore_camera = true});
    });
    CHECK(item(0).x == 50 && item(1).x == 100);
    nnh::shutdown();
}

NNH_TEST(each_drawn_character_counts_as_a_draw) {
    restart();
    frame_with([] { nnh::gfx::draw_text(Screen::Top, {0, 0}, "abc de", {.shadow = true}); });
    CHECK(nnh::gfx::stats().draws_last_frame == 10); // 5 characters, each with its shadow
    nnh::shutdown();
}

NNH_TEST(measure_text_gives_the_size_on_every_system) {
    CHECK(nnh::gfx::measure_text("Score: 100") == (nnh::Vec2{80, 10}));
    CHECK(nnh::gfx::measure_text("ação\nok", 2) == (nnh::Vec2{64, 40}));
    CHECK(nnh::gfx::measure_text("") == (nnh::Vec2{0, 0}));
}
