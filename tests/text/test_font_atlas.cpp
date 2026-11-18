// The built-in font laid out in a texture (text/font_atlas.hpp, spec 005 FR-1).

#include "test.hpp"
#include "text/builtin_font.hpp"
#include "text/font_atlas.hpp"
#include "text/glyphs.hpp"

using nnh::detail::build_font_atlas;
using nnh::detail::glyph_index;
using nnh::detail::glyph_region;
using nnh::detail::kAtlasSize;

namespace {

unsigned char g_atlas[kAtlasSize * kAtlasSize * 4];

const unsigned char* pixel(int x, int y) { return &g_atlas[(y * kAtlasSize + x) * 4]; }

} // namespace

NNH_TEST(atlas_is_a_power_of_two_square_holding_all_glyphs) {
    CHECK(kAtlasSize == 128);
    const nnh::Region last = glyph_region(nnh::detail::kGlyphCount - 1);
    CHECK(last.x + last.w <= kAtlasSize && last.y + last.h <= kAtlasSize);
}

NNH_TEST(glyph_regions_are_8x8_cells_16_per_row) {
    const nnh::Region a = glyph_region(0);
    CHECK(a.x == 0 && a.y == 0 && a.w == 8 && a.h == 8);
    const nnh::Region b = glyph_region(17);
    CHECK(b.x == 8 && b.y == 8);
}

NNH_TEST(atlas_pixels_are_white_with_the_glyph_in_alpha) {
    build_font_atlas(g_atlas);
    const nnh::Region a = glyph_region(glyph_index(U'A'));
    // 'A' row 1 is .###.. in columns 1-6: column 2 is set, column 1 isn't.
    const unsigned char* on = pixel(a.x + 2, a.y + 1);
    const unsigned char* off = pixel(a.x + 1, a.y + 1);
    CHECK(on[0] == 255 && on[1] == 255 && on[2] == 255 && on[3] == 255);
    // Transparent pixels stay white (alpha 0): no dark fringe when filtered, and a wrong byte order on
    // a console shows as solid boxes instead of hiding.
    CHECK(off[0] == 255 && off[1] == 255 && off[2] == 255 && off[3] == 0);
}

NNH_TEST(atlas_matches_the_font_bits_everywhere) {
    build_font_atlas(g_atlas);
    for (int g = 0; g < nnh::detail::kGlyphCount; ++g) {
        const nnh::Region r = glyph_region(g);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                const bool set = (nnh::detail::kBuiltinFont[g][row] & (0x80 >> col)) != 0;
                CHECK((pixel(r.x + col, r.y + row)[3] == 255) == set);
            }
        }
    }
}

NNH_TEST(atlas_space_below_the_glyphs_is_transparent) {
    build_font_atlas(g_atlas);
    CHECK(pixel(5, 120)[3] == 0);
    CHECK(pixel(127, 127)[3] == 0);
}
