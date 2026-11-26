// The .nnhfont writer (font_format.hpp): sections in file order, pages A4-packed then PackBits when smaller.

#include "font/font_format.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "font/page_codec.hpp"

namespace nnh::detail::font {
namespace {

/// Writes into `out` while `at` stays within `capacity` (a null `out` only counts).
struct Writer {
    std::uint8_t* out;
    std::size_t capacity;
    std::size_t at = 0;
    void bytes(const void* data, std::size_t n) {
        if (out != nullptr && at + n <= capacity && n > 0) std::memcpy(out + at, data, n);
        at += n;
    }
    void zeros(std::size_t n) {
        if (out != nullptr && at + n <= capacity) std::memset(out + at, 0, n);
        at += n;
    }
    void path(const char* p) {
        char buf[kPathBytes] = {};
        if (p != nullptr) std::strncpy(buf, p, kPathBytes - 1);
        bytes(buf, kPathBytes);
    }
};

std::size_t pixel_bytes(const PageSource& p) {
    const auto px = static_cast<std::size_t>(p.width) * static_cast<std::size_t>(p.height);
    return p.format == kPageA4 ? px / 2 : px * 4;
}

/// How a page is stored: PackBits when smaller, else raw.
PageRecord record_of(const PageSource& p) {
    PageRecord r;
    r.format = p.format;
    r.width = static_cast<std::uint16_t>(p.width);
    r.height = static_cast<std::uint16_t>(p.height);
    const std::size_t raw = pixel_bytes(p);
    const std::size_t packed = packbits_encode(p.data, raw, nullptr, 0);
    r.codec = packed < raw ? kCodecPackBits : kCodecRaw;
    r.bytes = static_cast<std::uint32_t>(r.codec == kCodecPackBits ? packed : raw);
    return r;
}

} // namespace

std::size_t page_decoded_bytes(const PageRecord& page) {
    const auto px = static_cast<std::size_t>(page.width) * static_cast<std::size_t>(page.height);
    return page.format == kPageA4 ? px / 2 : px * 4;
}

Header read_header(const std::uint8_t* data) {
    Header h;
    std::memcpy(&h, data, sizeof h);
    return h;
}

std::size_t font_write(const FontSource& s, std::uint8_t* out, std::size_t capacity) {
    const bool outlines = (s.header.flags & kFlagOutline) != 0 && s.outlines != nullptr;
    Header h = s.header;
    h.magic = kMagic;
    h.version = kVersion;
    h.glyph_count = s.glyph_count;
    h.kerning_count = s.kerning_count;
    h.page_count = static_cast<std::uint8_t>(s.page_count);
    h.flags = static_cast<std::uint16_t>(h.flags & ~kFlagBmp);
    for (std::uint32_t i = 0; i < s.glyph_count; ++i)
        if (s.codepoints[i] > 0xFF && s.codepoints[i] < 0x10000) h.flags = static_cast<std::uint16_t>(h.flags | kFlagBmp);
    h.fallback_count = static_cast<std::uint8_t>(s.fallback_count);
    h.off_codepoints = kHeaderSize;
    h.off_glyphs = h.off_codepoints + s.glyph_count * 4;
    h.off_outlines = outlines ? h.off_glyphs + s.glyph_count * 16 : 0;
    h.off_kerning = h.off_glyphs + s.glyph_count * 16 + (outlines ? s.glyph_count * 8 : 0);
    h.off_fallbacks = h.off_kerning + s.kerning_count * 4;
    h.off_licence = h.off_fallbacks + static_cast<std::uint32_t>(s.fallback_count) * kPathBytes;
    h.off_pages = h.off_licence + kPathBytes;
    h.off_page_data = h.off_pages + static_cast<std::uint32_t>(s.page_count) * sizeof(PageRecord);
    PageRecord records[kMaxPages] = {};
    std::uint32_t data_size = 0;
    for (int i = 0; i < s.page_count && i < kMaxPages; ++i) {
        records[i] = record_of(s.pages[i]);
        records[i].offset = h.off_page_data + data_size;
        data_size += records[i].bytes;
    }
    h.file_size = h.off_page_data + data_size;
    h.crc = 0;
    if (out == nullptr || capacity < h.file_size) return h.file_size;
    Writer w{out, capacity};
    w.bytes(&h, sizeof h);
    for (std::uint32_t i = 0; i < s.glyph_count; ++i) {
        const std::uint32_t cp = s.codepoints[i];
        w.bytes(&cp, 4);
    }
    w.bytes(s.glyphs, s.glyph_count * sizeof(GlyphRecord));
    if (outlines) w.bytes(s.outlines, s.glyph_count * sizeof(OutlineRecord));
    w.bytes(s.kerning, s.kerning_count * sizeof(KernPair));
    for (int i = 0; i < s.fallback_count; ++i) w.path(s.fallbacks[i]);
    w.path(s.licence);
    w.bytes(records, static_cast<std::size_t>(s.page_count) * sizeof(PageRecord));
    for (int i = 0; i < s.page_count && i < kMaxPages; ++i) {
        if (records[i].codec == kCodecPackBits) {
            w.at += packbits_encode(s.pages[i].data, pixel_bytes(s.pages[i]), out + w.at, records[i].bytes);
        } else {
            w.bytes(s.pages[i].data, records[i].bytes);
        }
    }
    const std::uint32_t crc = crc32(out, h.file_size);
    std::memcpy(out + kCrcOffset, &crc, 4);
    return h.file_size;
}

} // namespace nnh::detail::font
