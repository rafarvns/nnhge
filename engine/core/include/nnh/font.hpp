#pragma once
/// @file nnh/font.hpp
/// Game fonts (spec 029, decision D49): fonts the build converted from a TrueType/OpenType description (`<path>.font`) or
/// a BMFont (`<path>.fnt` + its pages) into `<path>.nnhfont`, loaded by path and drawn by gfx::draw_text through
/// TextOptions::font. Optional: a game that never calls load_font() links no font code. See docs/api/fonts.md.
///
/// @code
/// auto body = nnh::load_font("fonts/body_16");
/// if (!body) return false;
/// font_ = static_cast<nnh::Font&&>(body.value());
/// nnh::gfx::draw_text(nnh::Screen::Top, {8, 8}, "Olá, mundo", {.font = font_});
/// @endcode

#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/result.hpp"

namespace nnh {

struct MemTag;

inline constexpr int kMaxFonts = 32;         ///< loaded fonts at once (a path loaded twice counts once)
inline constexpr int kMaxFontPages = 8;      ///< glyph pages per font
inline constexpr int kMaxFallbackFonts = 3;  ///< fonts a font falls back to (one level)

/// A loaded font: the owner of one entry of the engine's font table (refcounted by path, so loading a path twice shares
/// one block and one set of pages). Movable, not copyable. Destroying the last owner frees the block and releases the
/// page textures after the GPU is done with them (D47). TextOptions and the UI hold a FontRef, which becomes stale
/// (Debug/Development assert, Release: the built-in font) once every owner is gone.
class Font {
public:
    Font() = default;
    ~Font();
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool valid() const;
    FontRef ref() const;
    /// Integer metrics at scale 1 (an empty Font: the built-in font's).
    FontMetrics metrics() const;
    /// The font itself has a glyph for `cp` (its fallbacks not counted).
    bool has(char32_t cp) const;
    /// The font or one of its fallbacks has a glyph for `cp`.
    bool covers(char32_t cp) const;
    /// The pen's move for `cp` at scale 1, in whole pixels (a missing character: the replacement glyph's or the
    /// hollow box's advance, what draw_text uses).
    int advance(char32_t cp) const;
    /// The kerning of the pair at scale 1; 0 when the pair has none (or the characters come from different fonts).
    int kerning(char32_t left, char32_t right) const;
    /// Debug/Development: the asset path it was loaded from; Release: "".
    const char* path() const;
    /// Debug/Development: logs its glyphs, pages, bytes and metrics (channel `font`); Release: nothing.
    void dump() const;

private:
    friend struct FontAccess;
    std::uint32_t id_ = 0;
};

/// Loads `<path>.nnhfont` from the game's assets (and its fallback fonts, one level): one block (tag `font` by default,
/// or `tag`) and its pages as textures (AppConfig::gfx.max_textures slots, accounted under `font`). Load time only
/// (allocates; a page is decoded into a temporary buffer freed at once). Errors: NotFound, Corrupted (the validator's
/// reason logged), OutOfMemory, LimitReached (32 fonts, or no texture slot left).
Result<Font> load_font(const char* path, MemTag* tag = nullptr);

namespace font {

/// Debug/Development: draws a font's pages (or one, `page` ≥ 0) side by side at `at`, scaled down to fit the screen,
/// with an outline per page and the glyph rectangles. Release: nothing. Only inside a frame.
void debug_draw_pages(Screen screen, FontRef font, Vec2 at, int page = -1);

} // namespace font
} // namespace nnh
