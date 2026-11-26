// Rasterising TrueType/OpenType outlines (ttf_raster.hpp) through the vendored stb_truetype. Built without fused
// multiply-add (tools/assetc/CMakeLists.txt), so every host bakes the same bitmaps.

#include "ttf_raster.hpp"

#include "stb_truetype.h"
#include "ttf_check.hpp"

namespace nnh::assetc {
namespace {

std::uint8_t level(int a, const RasterOptions& o) {
    if (!o.antialias) return a >= o.threshold ? 15 : 0;
    return static_cast<std::uint8_t>((a * 15 + 127) / 255);
}

} // namespace

std::string raster_font(const std::vector<unsigned char>& file, const RasterOptions& o, const std::set<char32_t>& wanted,
                        BakedFont& out, int& skipped, char32_t& first_skipped) {
    const std::string why = check_ttf(file.data(), file.size(), o.face);
    if (!why.empty()) return why;
    std::vector<unsigned char> padded(file);
    padded.resize(file.size() + 65536, 0);
    stbtt_fontinfo info;
    const int offset = stbtt_GetFontOffsetForIndex(padded.data(), o.face);
    if (offset < 0 || !stbtt_InitFont(&info, padded.data(), offset)) return "stb_truetype can't read the font";
    const int em = (padded[static_cast<std::size_t>(info.head) + 18] << 8) | padded[static_cast<std::size_t>(info.head) + 19];
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    out.size = o.size;
    out.ascent = scale_units(ascent, o.size, em);
    out.descent = scale_units(descent, o.size, em);
    out.line_height = out.ascent - out.descent + scale_units(gap, o.size, em);
    out.antialias = o.antialias;
    const float scale = stbtt_ScaleForMappingEmToPixels(&info, static_cast<float>(o.size));
    skipped = 0;
    first_skipped = 0;
    for (const char32_t cp : wanted) {
        const int g = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
        if (g == 0) {
            if (skipped++ == 0) first_skipped = cp;
            continue;
        }
        BakedGlyph b;
        b.cp = cp;
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&info, g, &adv, &lsb);
        b.advance = scale_units(adv, o.size, em);
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBox(&info, g, scale, scale, &x0, &y0, &x1, &y1);
        if (x1 > x0 && y1 > y0 && cp != U' ' && cp != 0xA0 && cp != 0x3000) {
            b.glyph.w = x1 - x0;
            b.glyph.h = y1 - y0;
            b.glyph.ox = x0;
            b.glyph.oy = out.ascent + y0;
            std::vector<unsigned char> cover(static_cast<std::size_t>(b.glyph.w * b.glyph.h));
            stbtt_MakeGlyphBitmap(&info, cover.data(), b.glyph.w, b.glyph.h, b.glyph.w, scale, scale, g);
            b.glyph.pixels.resize(cover.size());
            for (std::size_t i = 0; i < cover.size(); ++i) b.glyph.pixels[i] = level(cover[i], o);
        }
        out.glyphs.push_back(std::move(b));
    }
    for (const BakedGlyph& l : out.glyphs) { // kerning: pairs below U+3000
        if (l.cp >= 0x3000) continue;
        for (const BakedGlyph& r : out.glyphs) {
            if (r.cp >= 0x3000) continue;
            const int k = stbtt_GetCodepointKernAdvance(&info, static_cast<int>(l.cp), static_cast<int>(r.cp));
            const int px = scale_units(k, o.size, em);
            if (px != 0) out.kerning.push_back({l.cp, r.cp, px});
        }
    }
    return "";
}

} // namespace nnh::assetc
