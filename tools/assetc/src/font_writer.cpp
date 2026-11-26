// Writing a converted font (font_writer.hpp).

#include "font_writer.hpp"

#include <algorithm>

#include "font/font_format.hpp"
#include "font/page_codec.hpp"
#include "text/break_class.hpp"

namespace nnh::assetc {
namespace {

namespace ff = nnh::detail::font;

bool fits_i8(int v) { return v >= -128 && v <= 127; }

void blit(std::vector<std::uint8_t>& page, int page_w, int bpp, int x, int y, const Bitmap& b) {
    for (int row = 0; row < b.h; ++row) {
        std::copy_n(b.pixels.begin() + static_cast<long>(row) * b.w * bpp, static_cast<std::size_t>(b.w * bpp),
                    page.begin() + (static_cast<long>(y + row) * page_w + x) * bpp);
    }
}

int index_of(const BakedFont& f, char32_t cp) {
    const auto it = std::lower_bound(f.glyphs.begin(), f.glyphs.end(), cp,
                                     [](const BakedGlyph& g, char32_t c) { return g.cp < c; });
    return it != f.glyphs.end() && it->cp == cp ? static_cast<int>(it - f.glyphs.begin()) : -1;
}

} // namespace

std::string write_font(BakedFont& font, const std::vector<PageSize>& pages, std::vector<std::uint8_t>& out) {
    std::sort(font.glyphs.begin(), font.glyphs.end(), [](const BakedGlyph& a, const BakedGlyph& b) { return a.cp < b.cp; });
    if (font.glyphs.empty()) return "the font has no glyph (none of its characters is in the source)";
    if (font.glyphs.size() > ff::kMaxGlyphs) return "more than 65535 glyphs";
    const bool outlines = std::any_of(font.glyphs.begin(), font.glyphs.end(), [](const BakedGlyph& g) { return g.outline.w > 0; });
    std::vector<char32_t> cps;
    std::vector<ff::GlyphRecord> recs;
    std::vector<ff::OutlineRecord> outs;
    std::vector<ff::KernPair> pairs;
    for (const BakedGlyph& g : font.glyphs) {
        ff::GlyphRecord r;
        if (g.glyph.w > 255 || g.glyph.h > 255 || !fits_i8(g.glyph.ox) || !fits_i8(g.glyph.oy) || g.advance < 0 ||
            g.advance > 255) {
            return "a glyph's size or metrics are outside the format's ranges (size too large?)";
        }
        r.x = static_cast<std::uint16_t>(g.x);
        r.y = static_cast<std::uint16_t>(g.y);
        r.w = static_cast<std::uint8_t>(g.glyph.w);
        r.h = static_cast<std::uint8_t>(g.glyph.h);
        r.ox = static_cast<std::int8_t>(g.glyph.ox);
        r.oy = static_cast<std::int8_t>(g.glyph.oy);
        r.advance = static_cast<std::uint8_t>(g.advance);
        r.page = static_cast<std::uint8_t>(g.page);
        r.flags = nnh::detail::break_class(g.cp);
        ff::OutlineRecord o;
        if (g.outline.w > 0) {
            r.flags |= ff::kGlyphOutline;
            o.xp = static_cast<std::uint16_t>(g.outline_x | (g.outline_page << 12));
            o.y = static_cast<std::uint16_t>(g.outline_y);
            o.w = static_cast<std::uint8_t>(g.outline.w);
            o.h = static_cast<std::uint8_t>(g.outline.h);
            o.ox = static_cast<std::int8_t>(g.outline.ox);
            o.oy = static_cast<std::int8_t>(g.outline.oy);
        }
        std::vector<ff::KernPair> run;
        for (const KernPx& k : font.kerning) {
            if (k.left != g.cp || !fits_i8(k.amount)) continue;
            const int right = index_of(font, k.right);
            if (right >= 0) run.push_back({static_cast<std::uint16_t>(right), static_cast<std::int8_t>(k.amount), 0});
        }
        std::sort(run.begin(), run.end(), [](const ff::KernPair& a, const ff::KernPair& b) { return a.right < b.right; });
        if (run.size() > 255) run.resize(255);
        if (!run.empty()) {
            r.flags |= ff::kGlyphKerning;
            r.kern_first = static_cast<std::uint16_t>(pairs.size());
            r.kern_count = static_cast<std::uint8_t>(run.size());
            pairs.insert(pairs.end(), run.begin(), run.end());
        }
        cps.push_back(g.cp);
        recs.push_back(r);
        outs.push_back(o);
    }
    const int bpp = font.colour ? 4 : 1;
    std::vector<std::vector<std::uint8_t>> pixels(pages.size()), stored(pages.size());
    for (std::size_t p = 0; p < pages.size(); ++p) pixels[p].assign(static_cast<std::size_t>(pages[p].w * pages[p].h * bpp), 0);
    for (const BakedGlyph& g : font.glyphs) {
        if (g.glyph.w > 0) blit(pixels[static_cast<std::size_t>(g.page)], pages[static_cast<std::size_t>(g.page)].w, bpp, g.x, g.y, g.glyph);
        if (g.outline.w > 0) {
            blit(pixels[static_cast<std::size_t>(g.outline_page)], pages[static_cast<std::size_t>(g.outline_page)].w, bpp,
                 g.outline_x, g.outline_y, g.outline);
        }
    }
    std::vector<ff::PageSource> sources(pages.size());
    for (std::size_t p = 0; p < pages.size(); ++p) {
        if (font.colour) {
            stored[p] = pixels[p];
        } else {
            stored[p].resize(pixels[p].size() / 2);
            ff::a4_pack(pixels[p].data(), pixels[p].size(), stored[p].data());
        }
        sources[p] = {font.colour ? ff::kPageRgba8 : ff::kPageA4, pages[p].w, pages[p].h, stored[p].data()};
    }
    ff::FontSource s;
    s.header.size = static_cast<std::uint16_t>(font.size);
    s.header.line_height = static_cast<std::uint16_t>(font.line_height);
    s.header.ascent = static_cast<std::int16_t>(font.ascent);
    s.header.descent = static_cast<std::int16_t>(font.descent);
    s.header.flags = static_cast<std::uint16_t>((font.colour ? ff::kFlagColour : 0) | (outlines ? ff::kFlagOutline : 0) |
                                                (pairs.empty() ? 0 : ff::kFlagKerning) |
                                                (font.antialias ? ff::kFlagAntialias : 0));
    int replacement = font.replacement ? index_of(font, 0xFFFD) : -1;
    if (font.replacement && replacement < 0) replacement = index_of(font, U'?');
    s.header.replacement = replacement >= 0 ? static_cast<std::uint32_t>(replacement) : ff::kNoGlyph;
    const int ellipsis = index_of(font, 0x2026);
    s.header.ellipsis = ellipsis >= 0 ? static_cast<std::uint32_t>(ellipsis) : ff::kNoGlyph;
    s.codepoints = cps.data();
    s.glyphs = recs.data();
    s.outlines = outlines ? outs.data() : nullptr;
    s.glyph_count = static_cast<std::uint32_t>(cps.size());
    s.kerning = pairs.data();
    s.kerning_count = static_cast<std::uint32_t>(pairs.size());
    for (std::size_t i = 0; i < font.fallbacks.size() && i < 3; ++i) s.fallbacks[i] = font.fallbacks[i].c_str();
    s.fallback_count = static_cast<int>(font.fallbacks.size());
    s.licence = font.licence.c_str();
    s.pages = sources.data();
    s.page_count = static_cast<int>(sources.size());
    out.assign(ff::font_write(s, nullptr, 0), 0);
    ff::font_write(s, out.data(), out.size());
    const char* why = ff::font_validate(out.data(), out.size());
    return why == nullptr ? "" : std::string("the converted font is invalid: ") + why;
}

} // namespace nnh::assetc
