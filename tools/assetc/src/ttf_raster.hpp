#pragma once
// Rasterising TrueType/OpenType outlines (spec 029 FR-1, FR-4, FR-6; plan "Rasterising"): stb_truetype on a copy padded
// with 64 KiB of zeros (after ttf_check), metrics rounded once (scale_units), coverage rounded to 16 levels
// ((a × 15 + 127) / 255; anti-aliasing off: a ≥ threshold → 15), kerning from `kern` or GPOS pairs (pairs between glyphs
// below U+3000: ideographs don't kern). Tests: tests/test_ttf_raster.cpp.

#include <set>
#include <string>
#include <vector>

#include "font_glyphs.hpp"

namespace nnh::assetc {

struct RasterOptions {
    int face = 0;
    int size = 16;
    bool antialias = true;
    int threshold = 128;
};

/// Bakes the code points of `wanted` the font maps (the others are counted in `skipped`, the first in
/// `first_skipped`). `file` is the whole font file. @return "" or the problem.
std::string raster_font(const std::vector<unsigned char>& file, const RasterOptions& o, const std::set<char32_t>& wanted,
                        BakedFont& out, int& skipped, char32_t& first_skipped);

} // namespace nnh::assetc
