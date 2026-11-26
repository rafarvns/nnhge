// A font block's tables and derived indexes (font_block.hpp): the copy of the file's tables, the Latin-1 table, the BMP
// buckets and the lookup beyond the BMP.

#include <cstring>
#include <new>

#include "font/font_block.hpp"

namespace nnh::detail {
namespace {

constexpr std::size_t align8(std::size_t n) { return (n + 7) & ~std::size_t{7}; }

std::size_t bucket_bytes() { return align8((kBmpBuckets + 1) * sizeof(std::uint16_t)); }

} // namespace

std::size_t font_block_bytes(const font::Header& h) {
    return align8(sizeof(FontBlock)) + align8(h.off_page_data) + ((h.flags & font::kFlagBmp) != 0 ? bucket_bytes() : 0);
}

const FontBlock* font_block_init(void* memory, std::size_t bytes, const std::uint8_t* tables, std::size_t tables_size) {
    const font::Header h = font::read_header(tables);
    if (memory == nullptr || bytes < font_block_bytes(h) || tables_size < h.off_page_data) return nullptr;
    auto* base = static_cast<std::uint8_t*>(memory);
    auto* f = new (memory) FontBlock(); // parentheses: GCC 13 ICEs on brace value-init of structs with arrays
    std::uint8_t* file = base + align8(sizeof(FontBlock));
    std::memcpy(file, tables, h.off_page_data);
    f->header = h;
    f->codepoints = reinterpret_cast<const std::uint32_t*>(file + h.off_codepoints);
    f->glyphs = reinterpret_cast<const font::GlyphRecord*>(file + h.off_glyphs);
    f->outlines = (h.flags & font::kFlagOutline) != 0 ? reinterpret_cast<const font::OutlineRecord*>(file + h.off_outlines)
                                                      : nullptr;
    f->kerning = reinterpret_cast<const font::KernPair*>(file + h.off_kerning);
    for (int i = 0; i < h.fallback_count; ++i)
        f->fallbacks[i] = reinterpret_cast<const char*>(file + h.off_fallbacks + static_cast<std::size_t>(i) * font::kPathBytes);
    f->licence = reinterpret_cast<const char*>(file + h.off_licence);
    f->pages = reinterpret_cast<const font::PageRecord*>(file + h.off_pages);
    for (std::uint16_t& g : f->latin1) g = kNoGlyph16;
    std::uint32_t i = 0;
    for (; i < h.glyph_count && f->codepoints[i] < 256; ++i) f->latin1[f->codepoints[i]] = static_cast<std::uint16_t>(i);
    std::uint32_t end = i;
    while (end < h.glyph_count && f->codepoints[end] < 0x10000) ++end;
    f->bmp_end = end;
    if ((h.flags & font::kFlagBmp) != 0) {
        auto* buckets = reinterpret_cast<std::uint16_t*>(file + align8(h.off_page_data));
        std::uint32_t g = 0;
        for (int b = 0; b <= kBmpBuckets; ++b) { // the first glyph whose code point is at or above b × 32
            while (g < end && f->codepoints[g] < static_cast<std::uint32_t>(b) * 32u) ++g;
            buckets[b] = static_cast<std::uint16_t>(g);
        }
        f->buckets = buckets;
    }
    return f;
}

int find_glyph_beyond_bmp(const FontBlock& f, char32_t cp, int& compares) {
    int lo = static_cast<int>(f.bmp_end), hi = static_cast<int>(f.header.glyph_count) - 1;
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

} // namespace nnh::detail
