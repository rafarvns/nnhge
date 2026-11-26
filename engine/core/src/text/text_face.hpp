#pragma once
// A text face (spec 029 D49; plan "Text layout"): the built-in font, or a game font block with up to 3 fallbacks, at a
// scale. face_glyph() answers what a code point draws and how far it moves the pen, at scale 1 — the built-in font as
// spec 005 lays it out (every code point one 8-pixel cell, spaces drawn as nothing), a game font through its lookups,
// then its fallbacks, its replacement glyph and the hollow box. Inline: the run walk calls it for every code point.

#include <cstdint>

#include "font/font_block.hpp"
#include "nnh/gfx.hpp"
#include "text/break_class.hpp"
#include "text/glyphs.hpp"

namespace nnh::detail {

inline constexpr int kFaceFonts = 1 + font::kMaxFallbacks;

struct TextFace {
    const FontBlock* fonts[kFaceFonts] = {}; ///< [0] the font, then its fallbacks; none: the built-in font
    int count = 0;                           ///< fonts in use; 0 = the built-in font
    int scale = 1;                           ///< 1..8 (draw and measure multiply by it)
    bool builtin() const { return count == 0; }
};

/// What a code point draws in a face.
struct FaceGlyph {
    std::int8_t font = -1;    ///< index in TextFace::fonts, or −1 (the built-in font, or the hollow box)
    int glyph = -1;           ///< the font's record, or the built-in glyph index; −1: the hollow box
    int advance = 0;          ///< pen move at scale 1
    std::uint8_t breaks = 0;  ///< text/break_class.hpp bits
    bool draws = false;       ///< a bitmap (or the box) is drawn
    bool missing = false;     ///< no font of the face has it: the replacement glyph or the box (logged once)
};

inline int face_size(const TextFace& f) { return f.builtin() ? kFontCellSize : f.fonts[0]->header.size; }
inline int face_line_height(const TextFace& f) { return f.builtin() ? kFontLineHeight : f.fonts[0]->header.line_height; }
inline int face_ascent(const TextFace& f) { return f.builtin() ? kFontCellSize : f.fonts[0]->header.ascent; }
/// The hollow box's advance at a face's size: max(4, size / 2).
inline int face_box_advance(const TextFace& f) { return face_size(f) / 2 > 4 ? face_size(f) / 2 : 4; }

/// Spaces advance without drawing (U+0020, U+00A0, U+3000).
constexpr bool draws_nothing(char32_t c) { return c == U' ' || c == 0xA0 || c == 0x3000; }

inline FaceGlyph face_glyph(const TextFace& f, char32_t cp) {
    FaceGlyph g;
    if (f.builtin()) {
        g.glyph = glyph_index(cp);
        g.advance = kFontCellSize;
        g.breaks = break_class(cp);
        g.draws = !is_space(cp);
        return g;
    }
    g.breaks = break_class(cp);
    if ((g.breaks & kBreakZeroWidth) != 0) return g; // advances and draws nothing
    for (int i = 0; i < f.count; ++i) {
        const int index = find_glyph(*f.fonts[i], cp);
        if (index < 0) continue;
        const font::GlyphRecord& r = f.fonts[i]->glyphs[index];
        g.font = static_cast<std::int8_t>(i);
        g.glyph = index;
        g.advance = r.advance;
        g.breaks = static_cast<std::uint8_t>(r.flags & font::kGlyphBreakMask);
        g.draws = !draws_nothing(cp) && r.w > 0 && r.h > 0;
        return g;
    }
    if (draws_nothing(cp)) { // a space the fonts lack: the font's space, else half its size
        const FaceGlyph space = cp == U' ' ? FaceGlyph{} : face_glyph(f, U' ');
        g.advance = space.font >= 0 ? space.advance : face_size(f) / 2;
        if (cp == 0x3000) g.advance = face_size(f);
        return g;
    }
    g.missing = true;
    const std::uint32_t replacement = f.fonts[0]->header.replacement;
    if (replacement != font::kNoGlyph) {
        const font::GlyphRecord& r = f.fonts[0]->glyphs[replacement];
        g.font = 0;
        g.glyph = static_cast<int>(replacement);
        g.advance = r.advance;
        g.draws = r.w > 0 && r.h > 0;
        return g;
    }
    g.advance = face_box_advance(f); // the hollow box
    g.draws = true;
    return g;
}

/// Kerning between two consecutive glyphs: only within one font (0 across fonts, for the box or the built-in font).
inline int face_kerning(const TextFace& f, const FaceGlyph& prev, const FaceGlyph& cur) {
    if (prev.font < 0 || prev.font != cur.font || prev.glyph < 0 || cur.glyph < 0) return 0;
    return kerning(*f.fonts[prev.font], prev.glyph, cur.glyph);
}

} // namespace nnh::detail
