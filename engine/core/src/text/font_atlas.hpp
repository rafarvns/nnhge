#pragma once
// The built-in font laid out in one texture: 16 glyphs per row of 8x8 cells in a 128x128 RGBA image.
// Pure; unit tested in tests/text/test_font_atlas.cpp.

#include "nnh/math.hpp"

namespace nnh::detail {

inline constexpr int kAtlasSize = 128; // power of two (3DS texture rule)
inline constexpr int kAtlasGlyphsPerRow = 16;

/// The cell of a glyph in the atlas.
constexpr Region glyph_region(int glyph) {
    return Region{(glyph % kAtlasGlyphsPerRow) * 8, (glyph / kAtlasGlyphsPerRow) * 8, 8, 8};
}

/// Writes the atlas (kAtlasSize x kAtlasSize RGBA pixels, top row first): white everywhere, alpha 255
/// on the glyphs' pixels and 0 elsewhere. Color comes from the draw's tint.
void build_font_atlas(unsigned char* rgba);

} // namespace nnh::detail
