#pragma once
// A loaded font's block (spec 029 D49; plan "The font table and loading"): one allocation holding this struct, a copy
// of the file's tables [0, off_page_data) and, for fonts with glyphs above U+00FF, 2049 BMP buckets. Lookups are inline:
// a Latin-1 table read, or a bucket of 32 code points (cp >> 5) searched in at most ⌈log2⌉ + 1 compares (a full bucket
// is read directly), or a binary search beyond the BMP (font_index.cpp). Pure, no engine includes.
// Tests: tests/font/test_font_lookup.cpp.

#include <cstddef>
#include <cstdint>

#include "font/font_format.hpp"

namespace nnh::detail {

inline constexpr std::uint16_t kNoGlyph16 = 0xFFFF;
inline constexpr int kBmpBuckets = 2048; ///< 0x10000 / 32

struct FontBlock {
    font::Header header{};
    const std::uint32_t* codepoints = nullptr;
    const font::GlyphRecord* glyphs = nullptr;
    const font::OutlineRecord* outlines = nullptr; ///< null without kFlagOutline
    const font::KernPair* kerning = nullptr;
    const char* fallbacks[font::kMaxFallbacks] = {};
    const char* licence = nullptr;
    const font::PageRecord* pages = nullptr;
    const std::uint16_t* buckets = nullptr; ///< kBmpBuckets + 1 first indexes, or null (no glyph above U+00FF)
    std::uint32_t bmp_end = 0;              ///< the first glyph index at or above U+10000
    std::uint16_t latin1[256] = {};         ///< glyph index or kNoGlyph16
    // Set by the loader (font_load.cpp): the pages' textures in gfx's table.
    std::int16_t page_slot[font::kMaxPages] = {};
    std::uint32_t page_generation[font::kMaxPages] = {};
    const char* name = nullptr;            ///< its asset path (logs), set by the loader
    std::uint8_t self_slot = 0;            ///< its font table slot (logs)
    std::uint8_t padding[7] = {};
};

/// Bytes of the block of a font with header `h` (this struct, the tables, the buckets when needed).
std::size_t font_block_bytes(const font::Header& h);

/// Builds a block in `memory` (`bytes` = font_block_bytes(), 8-byte aligned) from the validated tables
/// (font_validate_tables) of `tables_size` = header.off_page_data bytes. @return the block, or nullptr if `bytes` is
/// too small. Never allocates.
const FontBlock* font_block_init(void* memory, std::size_t bytes, const std::uint8_t* tables, std::size_t tables_size);

/// Lookup beyond the BMP (code points ≥ U+10000): a binary search of [bmp_end, glyph_count).
int find_glyph_beyond_bmp(const FontBlock& f, char32_t cp, int& compares);

/// The glyph index of `cp`, or −1; `compares` counts the code points compared (tests).
inline int find_glyph_counted(const FontBlock& f, char32_t cp, int& compares) {
    if (cp < 256) {
        const std::uint16_t g = f.latin1[cp];
        return g == kNoGlyph16 ? -1 : g;
    }
    if (cp >= 0x10000) return find_glyph_beyond_bmp(f, cp, compares);
    if (f.buckets == nullptr) return -1;
    int lo = f.buckets[cp >> 5], hi = f.buckets[(cp >> 5) + 1] - 1;
    if (hi - lo == 31) return lo + static_cast<int>(cp & 31u); // a full bucket: every code point is there
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        const std::uint32_t at = f.codepoints[mid];
        ++compares;
        if (cp < at) hi = mid - 1;
        else if (cp > at) lo = mid + 1;
        else return mid;
    }
    return -1;
}

inline int find_glyph(const FontBlock& f, char32_t cp) {
    int compares = 0;
    return find_glyph_counted(f, cp, compares);
}

/// The kerning of the pair (left, right) of glyph indexes, in pixels at scale 1; 0 when the left glyph has no pair.
inline int kerning(const FontBlock& f, int left, int right) {
    const font::GlyphRecord& g = f.glyphs[left];
    if ((g.flags & font::kGlyphKerning) == 0) return 0;
    int lo = g.kern_first, hi = g.kern_first + g.kern_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) >> 1;
        const int r = f.kerning[mid].right;
        if (right < r) hi = mid - 1;
        else if (right > r) lo = mid + 1;
        else return f.kerning[mid].amount;
    }
    return 0;
}

} // namespace nnh::detail
