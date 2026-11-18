// Measuring text and placing its glyphs (text/layout.hpp, spec 005 FR-3, FR-4).

#include "test.hpp"
#include "text/glyphs.hpp"
#include "text/layout.hpp"

using nnh::TextAlign;
using nnh::Vec2;
using nnh::detail::glyph_index;
using nnh::detail::GlyphPlacement;
using nnh::detail::layout_text;
using nnh::detail::measure;

namespace {

struct Placed {
    int count = 0;
    GlyphPlacement g[32];
};

Placed place(const char* text, Vec2 origin = {}, int scale = 1, TextAlign align = TextAlign::Left) {
    Placed p;
    layout_text(text, origin, scale, align, [&p](const GlyphPlacement& g) {
        if (p.count < 32) p.g[p.count] = g;
        ++p.count;
    });
    return p;
}

} // namespace

NNH_TEST(measure_counts_characters_and_lines) {
    CHECK(measure("Hello", 1) == (Vec2{40, 10}));
    CHECK(measure("ab\nlonger", 1) == (Vec2{48, 20}));
    CHECK(measure("ação", 1) == (Vec2{32, 10})); // characters, not bytes
}

NNH_TEST(measure_scales) {
    CHECK(measure("Hi", 3) == (Vec2{48, 30}));
}

NNH_TEST(measure_of_nothing_is_zero) {
    CHECK(measure("", 1) == (Vec2{0, 0}));
    CHECK(measure(nullptr, 1) == (Vec2{0, 0}));
}

NNH_TEST(a_trailing_newline_adds_an_empty_line) {
    CHECK(measure("ab\n", 1) == (Vec2{16, 20}));
}

NNH_TEST(scale_is_clamped_to_1_through_8) {
    CHECK(measure("a", 0) == (Vec2{8, 10}));
    CHECK(measure("a", 100) == (Vec2{64, 80}));
}

NNH_TEST(carriage_returns_take_no_room) {
    CHECK(measure("ab\r\ncd", 1) == (Vec2{16, 20}));
}

NNH_TEST(glyphs_are_placed_left_to_right_and_spaces_are_skipped) {
    const Placed p = place("A B", {10, 20});
    CHECK(p.count == 2);
    CHECK(p.g[0].glyph == glyph_index(U'A') && p.g[0].x == 10 && p.g[0].y == 20);
    CHECK(p.g[1].glyph == glyph_index(U'B') && p.g[1].x == 26 && p.g[1].y == 20);
}

NNH_TEST(new_lines_start_under_the_first_line) {
    const Placed p = place("a\nb", {5, 5}, 2);
    CHECK(p.count == 2);
    CHECK(p.g[1].x == 5 && p.g[1].y == 25); // line height 10 x scale 2
}

NNH_TEST(center_and_right_alignment_work_per_line) {
    const Placed c = place("abcd\nab", {100, 0}, 1, TextAlign::Center);
    CHECK(c.g[0].x == 84);  // 32 wide: starts 16 left of 100
    CHECK(c.g[4].x == 92);  // second line, 16 wide: starts 8 left
    const Placed r = place("abcd\nab", {100, 0}, 1, TextAlign::Right);
    CHECK(r.g[0].x == 68);  // ends at 100
    CHECK(r.g[4].x == 84);
}

NNH_TEST(unknown_characters_are_placed_with_the_replacement_glyph) {
    const Placed p = place("\xE2\x82\xAC!"); // euro sign, then '!'
    CHECK(p.count == 2);
    CHECK(p.g[0].glyph == nnh::detail::kReplacementGlyph && p.g[1].x == 8);
}

NNH_TEST(nothing_is_placed_for_empty_text) {
    CHECK(place("").count == 0);
    CHECK(place(nullptr).count == 0);
    CHECK(place("   \n ").count == 0);
}
