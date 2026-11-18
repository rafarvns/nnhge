#pragma once
// Code points → glyphs of the built-in font. Pure; unit tested in tests/text/test_glyphs.cpp.
//
// Glyph order: 0-94 printable ASCII (32-126), 95 the replacement glyph, 96-191 Latin-1 (160-255).

namespace nnh::detail {

inline constexpr int kGlyphCount = 192;
inline constexpr int kReplacementGlyph = 95;

/// The glyph for `c`; characters the font doesn't have (and controls) get kReplacementGlyph.
constexpr int glyph_index(char32_t c) {
    if (c >= 32 && c <= 126) return static_cast<int>(c - 32);
    if (c >= 160 && c <= 255) return 96 + static_cast<int>(c - 160);
    return kReplacementGlyph;
}

/// Spaces advance without drawing anything.
constexpr bool is_space(char32_t c) { return c == U' ' || c == 0xA0; }

} // namespace nnh::detail
