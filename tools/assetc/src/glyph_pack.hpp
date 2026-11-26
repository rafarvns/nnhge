#pragma once
// Packing glyph bitmaps into pages (spec 029 FR-2; plan "Packing"): sorted by height, then width, then code point, on
// shelves left to right with `padding` pixels around each; a page is the smallest of 64×64, 128×64, 128×128 … 1024×1024
// that holds what is left (or the forced side), at most 8 pages. Outline bitmaps are packed like glyphs. Deterministic.
// Also the outline baking (a disk dilation of the coverage). Tests: tests/test_glyph_pack.cpp.

#include <string>

#include "font_glyphs.hpp"

namespace nnh::assetc {

struct PageSize {
    int w = 0, h = 0;
};

/// Places every glyph (and outline) of `font`; `pages` gets each page's size. @return "" or the refusal
/// ("<N> glyphs at <size> px need <P> pages of 1024x1024 (limit 8): reduce the size or the character set").
std::string pack_glyphs(BakedFont& font, std::vector<PageSize>& pages);

/// The outline of a mono glyph: its coverage dilated by a disk of radius `r` (max over offsets with dx² + dy² ≤ r² + r),
/// r pixels larger on every side.
Bitmap bake_outline(const Bitmap& glyph, int r);

} // namespace nnh::assetc
