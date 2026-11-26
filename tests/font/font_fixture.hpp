#pragma once
// Fonts built in memory for the core tests (spec 029 plan "Test strategy"): glyph tables written by hand and turned into
// `.nnhfont` bytes by the shared writer (font_format.cpp), so core tests don't depend on the rasteriser. Pages are A4
// (or RGBA8) with each glyph's rectangle filled by a pattern of levels 1..15.

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "font/font_format.hpp"
#include "font/page_codec.hpp"
#include "text/break_class.hpp"

namespace font_test {

namespace ff = nnh::detail::font;

struct Glyph {
    char32_t cp = 0;
    ff::GlyphRecord rec{};
    ff::OutlineRecord outline{};
    bool has_outline = false;
};

struct FontBuilder {
    int size = 16, line_height = 20, ascent = 15, descent = -4;
    std::uint16_t extra_flags = 0;
    int page_side = 64, page_height = 64, page_count = 1;
    bool colour = false;
    std::vector<Glyph> glyphs;
    std::vector<std::pair<std::pair<char32_t, char32_t>, int>> kerns;
    std::vector<std::string> fallbacks;
    std::string licence;
    bool no_replacement = false;

    /// A glyph whose bitmap is (x, y, w, h) of `page`, drawn at pen + (ox, oy), advancing `advance`.
    FontBuilder& glyph(char32_t cp, int x, int y, int w, int h, int ox, int oy, int advance, int page = 0) {
        Glyph g;
        g.cp = cp;
        g.rec.x = static_cast<std::uint16_t>(x);
        g.rec.y = static_cast<std::uint16_t>(y);
        g.rec.w = static_cast<std::uint8_t>(w);
        g.rec.h = static_cast<std::uint8_t>(h);
        g.rec.ox = static_cast<std::int8_t>(ox);
        g.rec.oy = static_cast<std::int8_t>(oy);
        g.rec.advance = static_cast<std::uint8_t>(advance);
        g.rec.page = static_cast<std::uint8_t>(page);
        glyphs.push_back(g);
        return *this;
    }
    /// The last glyph's outline bitmap.
    FontBuilder& outline(int x, int y, int w, int h, int ox, int oy, int page = 0) {
        Glyph& g = glyphs.back();
        g.has_outline = true;
        g.outline.xp = static_cast<std::uint16_t>(x | (page << 12));
        g.outline.y = static_cast<std::uint16_t>(y);
        g.outline.w = static_cast<std::uint8_t>(w);
        g.outline.h = static_cast<std::uint8_t>(h);
        g.outline.ox = static_cast<std::int8_t>(ox);
        g.outline.oy = static_cast<std::int8_t>(oy);
        return *this;
    }
    FontBuilder& kern(char32_t left, char32_t right, int amount) {
        kerns.push_back({{left, right}, amount});
        return *this;
    }

    /// A simple Latin font: 'A'..'Z', 'a'..'z', '0'..'9', ' ', '.', '?', U+2026 in 8×10 cells of one 128×64 page.
    static FontBuilder latin() {
        FontBuilder b;
        b.page_side = 128;
        b.page_height = 64;
        int i = 0;
        auto add = [&](char32_t cp, int w, int adv) {
            b.glyph(cp, (i % 16) * 8, (i / 16) * 10, w, 9, 1, 3, adv);
            ++i;
        };
        for (char32_t c = U'A'; c <= U'Z'; ++c) add(c, 6, 8);
        for (char32_t c = U'a'; c <= U'z'; ++c) add(c, 5, 6);
        for (char32_t c = U'0'; c <= U'9'; ++c) add(c, 5, 7);
        add(U'.', 2, 3);
        add(U'?', 5, 6);
        add(0x2026, 7, 9);
        b.glyph(U' ', 0, 0, 0, 0, 0, 0, 4);
        return b;
    }

    std::vector<std::uint8_t> build() const {
        std::vector<Glyph> gs = glyphs;
        std::stable_sort(gs.begin(), gs.end(), [](const Glyph& a, const Glyph& b) { return a.cp < b.cp; });
        auto index_of = [&](char32_t cp) {
            for (std::size_t i = 0; i < gs.size(); ++i)
                if (gs[i].cp == cp) return static_cast<int>(i);
            return -1;
        };
        std::vector<char32_t> cps;
        std::vector<ff::GlyphRecord> recs;
        std::vector<ff::OutlineRecord> outs;
        std::vector<ff::KernPair> pairs;
        bool any_outline = false;
        for (const Glyph& g : gs) any_outline = any_outline || g.has_outline;
        for (std::size_t i = 0; i < gs.size(); ++i) {
            ff::GlyphRecord r = gs[i].rec;
            r.flags = nnh::detail::break_class(gs[i].cp);
            if (gs[i].has_outline) r.flags |= ff::kGlyphOutline;
            std::vector<ff::KernPair> run;
            for (const auto& k : kerns) {
                if (k.first.first != gs[i].cp || index_of(k.first.second) < 0) continue;
                ff::KernPair p;
                p.right = static_cast<std::uint16_t>(index_of(k.first.second));
                p.amount = static_cast<std::int8_t>(k.second);
                run.push_back(p);
            }
            std::sort(run.begin(), run.end(), [](const ff::KernPair& a, const ff::KernPair& b) { return a.right < b.right; });
            if (!run.empty()) {
                r.flags |= ff::kGlyphKerning;
                r.kern_first = static_cast<std::uint16_t>(pairs.size());
                r.kern_count = static_cast<std::uint8_t>(run.size());
                pairs.insert(pairs.end(), run.begin(), run.end());
            }
            cps.push_back(gs[i].cp);
            recs.push_back(r);
            outs.push_back(gs[i].outline);
        }
        // pages: each glyph's (and outline's) rectangle filled with levels 1..15
        const int bpp = colour ? 4 : 1;
        std::vector<std::vector<std::uint8_t>> pixels(static_cast<std::size_t>(page_count),
                                                     std::vector<std::uint8_t>(static_cast<std::size_t>(page_side * page_height * bpp), 0));
        auto fill = [&](int page, int x, int y, int w, int h) {
            if (page < 0 || page >= page_count) return;
            for (int yy = y; yy < y + h && yy < page_height; ++yy)
                for (int xx = x; xx < x + w && xx < page_side; ++xx) {
                    const auto level = static_cast<std::uint8_t>(1 + (xx * 3 + yy) % 15);
                    std::uint8_t* p = pixels[static_cast<std::size_t>(page)].data() + (yy * page_side + xx) * bpp;
                    if (colour) {
                        p[0] = static_cast<std::uint8_t>(level * 17);
                        p[1] = 40;
                        p[2] = 200;
                        p[3] = 255;
                    } else {
                        p[0] = level;
                    }
                }
        };
        for (const Glyph& g : gs) {
            fill(g.rec.page, g.rec.x, g.rec.y, g.rec.w, g.rec.h);
            if (g.has_outline) fill(g.outline.xp >> 12, g.outline.xp & 0xFFF, g.outline.y, g.outline.w, g.outline.h);
        }
        std::vector<ff::PageSource> pages(static_cast<std::size_t>(page_count));
        std::vector<std::vector<std::uint8_t>> stored(static_cast<std::size_t>(page_count));
        for (int p = 0; p < page_count; ++p) {
            const auto i = static_cast<std::size_t>(p);
            if (colour) {
                stored[i] = pixels[i];
            } else {
                stored[i].resize(pixels[i].size() / 2);
                nnh::detail::font::a4_pack(pixels[i].data(), pixels[i].size(), stored[i].data());
            }
            pages[i] = {colour ? ff::kPageRgba8 : ff::kPageA4, page_side, page_height, stored[i].data()};
        }
        ff::FontSource src;
        src.header.size = static_cast<std::uint16_t>(size);
        src.header.line_height = static_cast<std::uint16_t>(line_height);
        src.header.ascent = static_cast<std::int16_t>(ascent);
        src.header.descent = static_cast<std::int16_t>(descent);
        src.header.flags = static_cast<std::uint16_t>(extra_flags | (colour ? ff::kFlagColour : 0) |
                                                      (any_outline ? ff::kFlagOutline : 0) |
                                                      (pairs.empty() ? 0 : ff::kFlagKerning));
        const int repl = no_replacement ? -1 : (index_of(0xFFFD) >= 0 ? index_of(0xFFFD) : index_of(U'?'));
        src.header.replacement = repl >= 0 ? static_cast<std::uint32_t>(repl) : ff::kNoGlyph;
        src.header.ellipsis = index_of(0x2026) >= 0 ? static_cast<std::uint32_t>(index_of(0x2026)) : ff::kNoGlyph;
        src.codepoints = cps.data();
        src.glyphs = recs.data();
        src.outlines = any_outline ? outs.data() : nullptr;
        src.glyph_count = static_cast<std::uint32_t>(cps.size());
        src.kerning = pairs.data();
        src.kerning_count = static_cast<std::uint32_t>(pairs.size());
        for (std::size_t i = 0; i < fallbacks.size() && i < 3; ++i) src.fallbacks[i] = fallbacks[i].c_str();
        src.fallback_count = static_cast<int>(fallbacks.size());
        src.licence = licence.c_str();
        src.pages = pages.data();
        src.page_count = page_count;
        std::vector<std::uint8_t> out(ff::font_write(src, nullptr, 0));
        ff::font_write(src, out.data(), out.size());
        return out;
    }
};

} // namespace font_test
