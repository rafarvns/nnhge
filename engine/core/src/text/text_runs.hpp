#pragma once
// A line's pen walk (spec 029 FR-6, FR-7, FR-8, FR-12; plan "Text layout"): every code point of one line through a face
// — '\r' skipped, zero-width characters advance nothing (and keep the kerning pair across them), kerning only within
// one font — and the line's width: the pen after its last code point, but at least every drawn glyph's ink (ox + w),
// so the measured box covers every drawn pixel to the right. Pure; all values at scale 1. Tests:
// tests/text/test_text_runs.cpp.

#include "nnh/gfx.hpp"
#include "text/text_face.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

struct RunItem {
    FaceGlyph g;
    char32_t cp = 0;
    int byte = 0;     ///< offset of the code point from the run's start
    int pen = 0;      ///< the pen where it is drawn (kerning applied)
    int advance = 0;  ///< the pen's move after it
    bool draw = false;
};

/// The ink's right edge of a drawn item (the glyph's bitmap, or the hollow box).
inline int ink_right(const TextFace& f, const RunItem& it) {
    if (!it.draw) return it.pen;
    if (it.g.glyph < 0 || f.builtin()) return it.pen + it.advance; // the box and the built-in cells: inside the advance
    const font::GlyphRecord& r = f.fonts[it.g.font]->glyphs[it.g.glyph];
    return it.pen + r.ox + r.w;
}

/// Walks [text, end) — one line, no '\n' — calling fn(const RunItem&) for every code point but '\r'.
/// @return the line's width at scale 1.
template <typename Fn>
int walk_run(const TextFace& face, const char* text, const char* end, Fn&& fn) {
    int pen = 0, width = 0;
    FaceGlyph prev;
    for (const char* p = text; p < end;) {
        const char* at = p;
        const char32_t c = next_codepoint(p);
        if (c == 0) break;
        if (c == U'\r') continue;
        RunItem it;
        it.g = face_glyph(face, c);
        it.cp = c;
        it.byte = static_cast<int>(at - text);
        it.advance = it.g.advance;
        it.draw = it.g.draws;
        const bool zero = !face.builtin() && (it.g.breaks & kBreakZeroWidth) != 0;
        if (!zero) pen += face_kerning(face, prev, it.g);
        it.pen = pen;
        fn(static_cast<const RunItem&>(it));
        pen += it.advance;
        const int ink = ink_right(face, it);
        width = width > ink ? width : ink;
        if (!zero) prev = it.g;
    }
    return width > pen ? width : pen;
}

/// The width of a run (walk_run without a callback).
inline int run_width(const TextFace& face, const char* text, const char* end) {
    return walk_run(face, text, end, [](const RunItem&) {});
}

/// The x offset of a line `width` wide for an alignment, at scale 1: Center subtracts width / 2 rounded down.
constexpr int align_offset(TextAlign align, int width) {
    return align == TextAlign::Center ? -(width / 2) : (align == TextAlign::Right ? -width : 0);
}

} // namespace nnh::detail
