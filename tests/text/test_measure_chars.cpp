// Per-character measuring (spec 029 FR-8): bytes, pen positions after alignment, lines, advances and the capacity.

#include "../font/font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"

using namespace font_test;

NNH_TEST(measure_chars_of_the_builtin_font) {
    start_engine();
    nnh::TextCaret c[8];
    const int n = nnh::gfx::measure_chars("ab\nc\xC3\xA9", {.scale = 2}, c, 8);
    CHECK(n == 4);
    CHECK(c[0].byte == 0 && c[0].x == 0 && c[0].y == 0 && c[0].advance == 16 && c[0].line == 0);
    CHECK(c[1].byte == 1 && c[1].x == 16);
    CHECK(c[2].byte == 3 && c[2].x == 0 && c[2].y == 20 && c[2].line == 1);
    CHECK(c[3].byte == 4 && c[3].x == 16);
}

NNH_TEST(measure_chars_with_a_font_kerning_and_alignment) {
    start_engine();
    FontBuilder b;
    b.glyph(U' ', 0, 0, 0, 0, 0, 0, 4).glyph(U'A', 0, 0, 7, 9, 0, 2, 8).glyph(U'V', 8, 0, 7, 9, 0, 2, 8);
    b.kern(U'A', U'V', -1);
    add_font("fonts/k", b);
    auto f = nnh::load_font("fonts/k");
    CHECK(f.ok());
    if (!f) return;
    nnh::TextCaret c[4];
    const int n = nnh::gfx::measure_chars("AV", {.align = nnh::TextAlign::Right, .font = f.value()}, c, 4);
    CHECK(n == 2 && c[0].x == -15 && c[1].x == -8 && c[1].advance == 8); // width 15
    CHECK(nnh::gfx::measure_chars("A\xCC\x81V", {.font = f.value()}, c, 4) == 3);
    CHECK(c[1].byte == 1 && c[1].x == 8 && c[1].advance == 0 && c[2].byte == 3 && c[2].x == 7);
}

NNH_TEST(measure_chars_stops_at_the_capacity) {
    start_engine();
    nnh::TextCaret c[2];
    CHECK(nnh::gfx::measure_chars("abcdef", {}, c, 2) == 2);
    CHECK(nnh::gfx::measure_chars("abcdef", {}, c, 0) == 0);
    CHECK(nnh::gfx::measure_chars(nullptr, {}, c, 2) == 0);
}

NNH_TEST(measure_chars_clamps_positions_past_the_int16_range) {
    // review N5: a 600-character line at scale 8 ends at x = 38 336, past int16: clamped, never wrapped negative
    start_engine();
    static char line[601];
    for (int i = 0; i < 600; ++i) line[i] = 'W';
    static nnh::TextCaret c[600];
    CHECK(nnh::gfx::measure_chars(line, {.scale = 8}, c, 600) == 600);
    CHECK(c[400].x == 400 * 64 && c[599].x == 32767 && c[599].advance == 64);
    CHECK(nnh::gfx::measure_chars(line, {.scale = 8, .align = nnh::TextAlign::Right}, c, 600) == 600);
    CHECK(c[0].x == -32768 && c[599].x == -64);
    // 500 lines at scale 8: the last line's top is 39 920
    static char lines[1001];
    for (int i = 0; i < 1000; ++i) lines[i] = i % 2 == 0 ? 'a' : '\n';
    CHECK(nnh::gfx::measure_chars(lines, {.scale = 8}, c, 600) == 500);
    CHECK(c[100].y == 8000 && c[499].y == 32767 && c[499].line == 499);
}
