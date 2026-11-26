// The .nnhfont validator (font_format.hpp): bounded checks of every offset, size and count, in file order, then the
// pages' exact decoding and the CRC. Never allocates (pages are decoded into a small window and dropped).

#include <cstring>

#include "core/crc32.hpp"
#include "font/font_format.hpp"
#include "font/page_codec.hpp"

namespace nnh::detail::font {
namespace {

bool power_of_two_side(int v) { return v >= kMinPageSide && v <= kMaxPageSide && (v & (v - 1)) == 0; }

std::uint32_t u32(const std::uint8_t* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

/// A NUL within the 64 bytes and only printable ASCII before it.
bool path_ok(const std::uint8_t* p) {
    for (std::size_t i = 0; i < kPathBytes; ++i) {
        if (p[i] == 0) return true;
        if (p[i] < 0x20 || p[i] > 0x7E) return false;
    }
    return false;
}

const char* check_header(const Header& h, std::size_t file_size) {
    if (h.magic != kMagic) return "not a font (magic)";
    if (h.version != kVersion) return "unsupported version";
    if ((h.flags & ~kKnownFlags) != 0) return "unknown flags";
    if (h.file_size != file_size) return "file size mismatch";
    if (h.size == 0 || h.size > 255 || h.line_height == 0 || h.line_height > 255 || h.ascent < -128 ||
        h.ascent > 255 || h.descent > 0 || h.descent < -255) {
        return "metrics out of range";
    }
    if (h.glyph_count == 0 || h.glyph_count > kMaxGlyphs) return "glyph count out of range";
    if (h.kerning_count > kMaxKerningPairs) return "too many kerning pairs";
    if (h.page_count == 0 || h.page_count > kMaxPages) return "pages: count out of range";
    if (h.fallback_count > kMaxFallbacks) return "too many fallback fonts";
    if (h.replacement != kNoGlyph && h.replacement >= h.glyph_count) return "replacement glyph out of range";
    if (h.ellipsis != kNoGlyph && h.ellipsis >= h.glyph_count) return "ellipsis glyph out of range";
    const bool outlines = (h.flags & kFlagOutline) != 0;
    // sections in order, without gaps (64-bit sums: no wrap-around)
    std::uint64_t at = kHeaderSize;
    const auto next = [&](std::uint32_t offset, std::uint64_t bytes) {
        const bool ok = offset == at;
        at += bytes;
        return ok;
    };
    bool ok = next(h.off_codepoints, std::uint64_t{h.glyph_count} * 4);
    ok = ok && next(h.off_glyphs, std::uint64_t{h.glyph_count} * sizeof(GlyphRecord));
    ok = ok && (outlines ? next(h.off_outlines, std::uint64_t{h.glyph_count} * sizeof(OutlineRecord)) : h.off_outlines == 0);
    ok = ok && next(h.off_kerning, std::uint64_t{h.kerning_count} * sizeof(KernPair));
    ok = ok && next(h.off_fallbacks, std::uint64_t{h.fallback_count} * kPathBytes);
    ok = ok && next(h.off_licence, kPathBytes);
    ok = ok && next(h.off_pages, std::uint64_t{h.page_count} * sizeof(PageRecord));
    ok = ok && h.off_page_data == at && at <= file_size;
    for (const std::uint32_t r : h.reserved) ok = ok && r == 0;
    return ok && h.reserved0 == 0 ? nullptr : "bad section layout";
}

const char* check_glyph(const GlyphRecord& g, const PageRecord* pages, int page_count, std::uint32_t pairs) {
    if (g.page >= page_count) return "glyph page out of range";
    const PageRecord& p = pages[g.page];
    if (std::uint32_t{g.x} + g.w > p.width || std::uint32_t{g.y} + g.h > p.height) return "glyph outside its page";
    const bool kerns = (g.flags & kGlyphKerning) != 0;
    if (kerns != (g.kern_count > 0)) return "kerning flag mismatch";
    if (std::uint32_t{g.kern_first} + g.kern_count > pairs) return "kerning run outside the pairs";
    if ((g.flags & 0x80) != 0 || g.reserved != 0) return "unknown glyph flags";
    return nullptr;
}

const char* check_outline(const OutlineRecord& o, const PageRecord* pages, int page_count) {
    const int page = o.xp >> 12;
    if (page >= page_count) return "outline page out of range";
    const std::uint32_t x = o.xp & 0xFFFu;
    if (x + o.w > pages[page].width || std::uint32_t{o.y} + o.h > pages[page].height) return "outline outside its page";
    return nullptr;
}

const char* check_pages(const Header& h, const PageRecord* pages, std::size_t file_size) {
    std::uint64_t at = h.off_page_data;
    for (int i = 0; i < h.page_count; ++i) {
        const PageRecord& p = pages[i];
        if (!power_of_two_side(p.width) || !power_of_two_side(p.height)) return "bad page side";
        if (p.width > 2 * p.height || p.height > 2 * p.width) return "bad page side (not square or 2:1)";
        if (p.format != kPageA4 && p.format != kPageRgba8) return "unknown page format";
        if ((p.format == kPageRgba8) != ((h.flags & kFlagColour) != 0)) return "page format doesn't match the font";
        if (p.codec != kCodecRaw && p.codec != kCodecPackBits) return "unknown page codec";
        if (p.reserved != 0) return "bad page record";
        if (p.offset != at || std::uint64_t{p.offset} + p.bytes > file_size) return "page data outside the file";
        at += p.bytes;
    }
    return at == file_size ? nullptr : "page data doesn't end the file";
}

} // namespace


const char* font_validate_tables(const std::uint8_t* data, std::size_t size, std::size_t file_size) {
    if (data == nullptr || size < kHeaderSize) return "too small";
    const Header h = read_header(data);
    if (const char* why = check_header(h, file_size)) return why;
    if (size < h.off_page_data) return "too small";
    PageRecord pages[kMaxPages];
    std::memcpy(pages, data + h.off_pages, h.page_count * sizeof(PageRecord));
    if (const char* why = check_pages(h, pages, file_size)) return why;
    std::uint32_t prev = 0;
    bool bmp = false;
    for (std::uint32_t i = 0; i < h.glyph_count; ++i) {
        const std::uint32_t cp = u32(data + h.off_codepoints + i * 4);
        if (cp > 0x10FFFF) return "code point out of range";
        bmp = bmp || (cp > 0xFF && cp < 0x10000);
        if (i > 0 && cp <= prev) return "code points not ascending";
        prev = cp;
        GlyphRecord g;
        std::memcpy(&g, data + h.off_glyphs + i * sizeof g, sizeof g);
        if (const char* why = check_glyph(g, pages, h.page_count, h.kerning_count)) return why;
        if ((h.flags & kFlagOutline) != 0) {
            OutlineRecord o;
            std::memcpy(&o, data + h.off_outlines + i * sizeof o, sizeof o);
            if (const char* why = check_outline(o, pages, h.page_count)) return why;
        } else if ((g.flags & kGlyphOutline) != 0) {
            return "outline flag without outlines";
        }
        for (std::uint32_t k = 0; k < g.kern_count; ++k) {
            KernPair a;
            std::memcpy(&a, data + h.off_kerning + (std::uint32_t{g.kern_first} + k) * sizeof a, sizeof a);
            if (a.right >= h.glyph_count || a.reserved != 0) return "kerning pair out of range";
            if (k > 0) {
                KernPair before;
                std::memcpy(&before, data + h.off_kerning + (std::uint32_t{g.kern_first} + k - 1) * sizeof a, sizeof a);
                if (before.right >= a.right) return "kerning run not sorted";
            }
        }
    }
    if (bmp != ((h.flags & kFlagBmp) != 0)) return "BMP flag mismatch";
    for (int i = 0; i < h.fallback_count; ++i)
        if (!path_ok(data + h.off_fallbacks + static_cast<std::size_t>(i) * kPathBytes)) return "bad fallback path";
    if (!path_ok(data + h.off_licence)) return "bad licence path";
    return nullptr;
}

const char* font_validate(const std::uint8_t* data, std::size_t size) {
    if (const char* why = font_validate_tables(data, size, size)) return why;
    const Header h = read_header(data);
    PageRecord pages[kMaxPages];
    std::memcpy(pages, data + h.off_pages, h.page_count * sizeof(PageRecord));
    for (int i = 0; i < h.page_count; ++i) {
        const PageRecord& p = pages[i];
        const std::size_t want = page_decoded_bytes(p);
        if (p.codec == kCodecRaw) {
            if (p.bytes != want) return "page doesn't decode to its size";
            continue;
        }
        // decode in pieces of a stack window: the decoder needs the exact size, so count instead of storing
        std::size_t produced = 0, at = p.offset;
        const std::size_t end = p.offset + p.bytes;
        while (at < end) {
            const std::uint8_t c = data[at++];
            if (c < 128) {
                const std::size_t n = std::size_t{c} + 1;
                if (n > end - at) return "page doesn't decode to its size";
                at += n;
                produced += n;
            } else if (c > 128) {
                if (at >= end) return "page doesn't decode to its size";
                ++at;
                produced += 257 - std::size_t{c};
            } else {
                return "page doesn't decode to its size";
            }
            if (produced > want) return "page doesn't decode to its size";
        }
        if (produced != want) return "page doesn't decode to its size";
    }
    std::uint32_t crc = crc32_update(0, data, kCrcOffset);
    const std::uint8_t zero[4] = {};
    crc = crc32_update(crc, zero, 4);
    crc = crc32_update(crc, data + kCrcOffset + 4, size - kCrcOffset - 4);
    return crc == h.crc ? nullptr : "bad CRC";
}

} // namespace nnh::detail::font
