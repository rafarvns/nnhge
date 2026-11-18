// Code point → glyph of the built-in font (text/glyphs.hpp, spec 005 FR-2).

#include "test.hpp"
#include "text/glyphs.hpp"

using nnh::detail::glyph_index;
using nnh::detail::is_space;
using nnh::detail::kGlyphCount;
using nnh::detail::kReplacementGlyph;

NNH_TEST(printable_ascii_maps_to_the_first_glyphs) {
    CHECK(glyph_index(U' ') == 0);
    CHECK(glyph_index(U'A') == 'A' - 32);
    CHECK(glyph_index(U'~') == 94);
}

NNH_TEST(latin1_maps_after_the_replacement_glyph) {
    CHECK(kReplacementGlyph == 95);
    CHECK(glyph_index(0xA0) == 96);
    CHECK(glyph_index(U'ç') == 96 + (0xE7 - 0xA0));
    CHECK(glyph_index(U'ÿ') == kGlyphCount - 1);
}

NNH_TEST(characters_outside_the_font_use_the_replacement_glyph) {
    CHECK(glyph_index(0x20AC) == kReplacementGlyph); // euro sign
    CHECK(glyph_index(0x1F600) == kReplacementGlyph);
    CHECK(glyph_index(0x7F) == kReplacementGlyph);   // DEL
    CHECK(glyph_index(0x85) == kReplacementGlyph);   // C1 control
    CHECK(glyph_index(0x07) == kReplacementGlyph);   // bell
    CHECK(glyph_index(0xFFFD) == kReplacementGlyph);
}

NNH_TEST(space_and_no_break_space_are_spaces) {
    CHECK(is_space(U' ') && is_space(0xA0));
    CHECK(!is_space(U'A') && !is_space(0xFFFD));
}
