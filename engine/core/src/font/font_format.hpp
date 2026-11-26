#pragma once
// The converted font format `.nnhfont`, version 1 (spec 029 FR-2, D49; plan 029 "The .nnhfont format"). Little-endian,
// sections in this order with no gap: a 96-byte header, the code points (u32, strictly ascending), 16-byte glyph
// records, 8-byte outline records (one per glyph, when the font has outlines), 4-byte kerning pairs (a run per left
// glyph, sorted by the right glyph), the fallback paths (64 bytes each), the licence path (64 bytes), the page records
// (16 bytes each), then the page data in page order. So a loader reads the tables [0, page_data) at once and each page
// after the other, checking the CRC-32 as it goes. Pure, no engine includes: nnh-assetc writes fonts with
// font_write(), the console reads them with font_validate.cpp. Tests: tests/font/test_font_format.cpp.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::font {

inline constexpr std::uint32_t kMagic = 0x464E484Eu; // "NNHF"
inline constexpr std::uint16_t kVersion = 1;
inline constexpr std::size_t kHeaderSize = 96;
inline constexpr std::size_t kPathBytes = 64;
inline constexpr int kMaxPages = 8;
inline constexpr int kMaxFallbacks = 3;
inline constexpr std::uint32_t kMaxGlyphs = 65535;
inline constexpr std::uint32_t kMaxKerningPairs = 65535;
inline constexpr int kMinPageSide = 8;
inline constexpr int kMaxPageSide = 1024;
inline constexpr std::uint32_t kNoGlyph = 0xFFFFFFFFu;

// Header flags.
inline constexpr std::uint16_t kFlagColour = 1;    ///< RGBA8 pages (a pre-coloured BMFont)
inline constexpr std::uint16_t kFlagOutline = 2;   ///< outline records follow the glyph records
inline constexpr std::uint16_t kFlagKerning = 4;   ///< some glyph has a kerning run
inline constexpr std::uint16_t kFlagAntialias = 8; ///< informative (the converter's setting)
inline constexpr std::uint16_t kFlagBmp = 16;      ///< a glyph in U+0100..U+FFFF (the block gets BMP buckets); set by font_write
inline constexpr std::uint16_t kKnownFlags = 31;

// Glyph record flags: bits 0–4 the break bits (text/break_class.hpp), then:
inline constexpr std::uint8_t kGlyphOutline = 32; ///< its outline record has a bitmap
inline constexpr std::uint8_t kGlyphKerning = 64; ///< kern_count > 0
inline constexpr std::uint8_t kGlyphBreakMask = 31;

// Page formats and codecs.
inline constexpr std::uint8_t kPageA4 = 0;    ///< 4-bit coverage, two pixels a byte (low nibble first)
inline constexpr std::uint8_t kPageRgba8 = 1; ///< 4 bytes a pixel (R, G, B, A)
inline constexpr std::uint8_t kCodecRaw = 0;
inline constexpr std::uint8_t kCodecPackBits = 1;

struct Header {
    std::uint32_t magic = kMagic;
    std::uint16_t version = kVersion;
    std::uint16_t flags = 0;
    std::uint16_t size = 0;        ///< em size in pixels
    std::uint16_t line_height = 0; ///< ascent − descent + line gap
    std::int16_t ascent = 0;       ///< baseline from the line's top
    std::int16_t descent = 0;      ///< ≤ 0: below the baseline
    std::uint32_t glyph_count = 0;
    std::uint32_t kerning_count = 0; ///< pairs
    std::uint8_t page_count = 0;
    std::uint8_t fallback_count = 0;
    std::uint16_t reserved0 = 0;
    std::uint32_t replacement = kNoGlyph; ///< glyph drawn for a missing code point (U+FFFD or '?'), or kNoGlyph
    std::uint32_t ellipsis = kNoGlyph;    ///< the glyph of U+2026, or kNoGlyph
    std::uint32_t file_size = 0;
    std::uint32_t crc = 0; ///< CRC-32 of the whole file with this field zero
    std::uint32_t off_codepoints = 0;
    std::uint32_t off_glyphs = 0;
    std::uint32_t off_outlines = 0; ///< 0 without kFlagOutline
    std::uint32_t off_kerning = 0;
    std::uint32_t off_fallbacks = 0;
    std::uint32_t off_licence = 0;
    std::uint32_t off_pages = 0;
    std::uint32_t off_page_data = 0;
    std::uint32_t reserved[5] = {};
};
static_assert(sizeof(Header) == kHeaderSize, "the file's header");
inline constexpr std::size_t kCrcOffset = 40;

/// A glyph's bitmap in its page and its metrics, in pixels at scale 1. `ox`, `oy`: the bitmap's top-left from the pen
/// on the line's top; `advance`: the pen's move.
struct GlyphRecord {
    std::uint16_t x = 0, y = 0;
    std::uint8_t w = 0, h = 0;
    std::int8_t ox = 0, oy = 0;
    std::uint8_t advance = 0;
    std::uint8_t page = 0;
    std::uint8_t flags = 0;      ///< break bits, kGlyphOutline, kGlyphKerning
    std::uint8_t kern_count = 0; ///< pairs of its run
    std::uint16_t kern_first = 0;
    std::uint16_t reserved = 0;
};
static_assert(sizeof(GlyphRecord) == 16, "16-byte glyph records");

/// A glyph's outline bitmap (drawn under it in TextOptions::outline_color). `xp` = x | page << 12.
struct OutlineRecord {
    std::uint16_t xp = 0, y = 0;
    std::uint8_t w = 0, h = 0;
    std::int8_t ox = 0, oy = 0;
};
static_assert(sizeof(OutlineRecord) == 8, "8-byte outline records");

/// A kerning pair of the left glyph's run: the right glyph's index and the pen adjustment in pixels.
struct KernPair {
    std::uint16_t right = 0;
    std::int8_t amount = 0;
    std::uint8_t reserved = 0;
};
static_assert(sizeof(KernPair) == 4, "4-byte kerning pairs");

struct PageRecord {
    std::uint8_t format = kPageA4;
    std::uint8_t codec = kCodecRaw;
    std::uint16_t width = 0, height = 0;
    std::uint16_t reserved = 0;
    std::uint32_t offset = 0; ///< from the file's start
    std::uint32_t bytes = 0;  ///< stored (encoded) bytes
};
static_assert(sizeof(PageRecord) == 16, "16-byte page records");

/// Decoded bytes of a page: A4 = width × height / 2, RGBA8 = width × height × 4.
std::size_t page_decoded_bytes(const PageRecord& page);

/// A page for font_write(): its decoded bytes, row-major, top row first — A4: a4_pack()ed (page_codec.hpp), width ×
/// height / 2 bytes; RGBA8: width × height × 4 bytes.
struct PageSource {
    std::uint8_t format = kPageA4;
    int width = 0, height = 0;
    const std::uint8_t* data = nullptr;
};

/// Everything font_write() needs, in memory (nnh-assetc's converter and the core tests build fonts with it).
struct FontSource {
    Header header{};                       ///< flags, size, line_height, ascent, descent, replacement, ellipsis
    const char32_t* codepoints = nullptr;  ///< glyph_count, strictly ascending
    const GlyphRecord* glyphs = nullptr;   ///< glyph_count
    const OutlineRecord* outlines = nullptr; ///< glyph_count when header.flags has kFlagOutline
    std::uint32_t glyph_count = 0;
    const KernPair* kerning = nullptr;
    std::uint32_t kerning_count = 0;
    const char* fallbacks[kMaxFallbacks] = {};
    int fallback_count = 0;
    const char* licence = nullptr; ///< "" or nullptr: none
    const PageSource* pages = nullptr;
    int page_count = 0;
};

/// Writes the font into `out` when `capacity` is enough (pages stored with PackBits when it is smaller, else raw) and
/// returns its size; returns the size without writing when `out` is null or too small. Doesn't validate: run
/// font_validate() on the result.
std::size_t font_write(const FontSource& source, std::uint8_t* out, std::size_t capacity);

/// Checks a whole file (plan 029's list: every offset, size and count, ascending code points, records inside their
/// pages, kerning runs inside the pairs and sorted, page sides, every page decoding to its exact size, the CRC).
/// @return nullptr when valid, else the reason (a static string). Bounded; never allocates.
const char* font_validate(const std::uint8_t* data, std::size_t size);

/// The tables part [0, header.off_page_data) of a file of `file_size` bytes: everything but the page data and the CRC.
const char* font_validate_tables(const std::uint8_t* data, std::size_t size, std::size_t file_size);

/// Reads the header of at least kHeaderSize bytes (no check).
Header read_header(const std::uint8_t* data);

} // namespace nnh::detail::font
