#pragma once
// The font converter's model (spec 029 D49): glyph bitmaps with integer metrics, made by the rasteriser (ttf_raster) or
// read from a BMFont (bmfont_convert), packed into pages (glyph_pack) and written as `.nnhfont` (font_writer).

#include <cstdint>
#include <string>
#include <vector>

namespace nnh::assetc {

struct Bitmap {
    int w = 0, h = 0;
    int ox = 0, oy = 0;              ///< top-left from the pen on the line's top, pixels
    std::vector<std::uint8_t> pixels; ///< levels 0..15 (mono) or RGBA8 (colour), row-major
};

struct BakedGlyph {
    char32_t cp = 0;
    int advance = 0;
    Bitmap glyph;
    Bitmap outline; ///< w == 0: none
    // placement (glyph_pack)
    int page = 0, x = 0, y = 0;
    int outline_page = 0, outline_x = 0, outline_y = 0;
};

struct KernPx {
    char32_t left = 0, right = 0;
    int amount = 0; ///< pixels, scale 1
};

struct BakedFont {
    int size = 0, line_height = 0, ascent = 0, descent = 0;
    bool colour = false, antialias = true;
    std::vector<BakedGlyph> glyphs; ///< sorted by code point once written
    std::vector<KernPx> kerning;
    std::vector<std::string> fallbacks;
    std::string licence;            ///< "" for none ("own")
    bool replacement = true;        ///< pick U+FFFD or '?' as the replacement glyph
    int page_side = 0;              ///< forced page side (0: the smallest that fits)
    int padding = 1;
};

/// Rounds units × size / em half away from zero, in 64-bit integers (the converter's one rounding rule for metrics).
inline int scale_units(long long units, int size, int em) {
    const long long n = units * size * 2, d = static_cast<long long>(em) * 2;
    return static_cast<int>(n >= 0 ? (n + em) / d : -((-n + em) / d));
}

} // namespace nnh::assetc
