// Fuzz target of the .nnhfont validator (spec 029 FR-20 for fonts, X): see fuzz_targets.hpp.

#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/crc32.hpp"
#include "font/font_format.hpp"
#include "font/page_codec.hpp"
#include "fuzz_targets.hpp"

namespace nnh::fuzz {
namespace {

namespace ff = detail::font;

std::vector<std::uint8_t> with_size_and_crc(const std::uint8_t* data, std::size_t size) {
    std::vector<std::uint8_t> f(data, data + size);
    if (size < ff::kHeaderSize) return f;
    const auto n = static_cast<std::uint32_t>(size);
    std::memcpy(f.data() + 36, &n, 4); // file_size
    std::memset(f.data() + ff::kCrcOffset, 0, 4);
    const std::uint32_t crc = detail::crc32(f.data(), f.size());
    std::memcpy(f.data() + ff::kCrcOffset, &crc, 4);
    return f;
}

/// Everything a loader reads of an accepted font stays in bounds (ASan watches; a violated invariant aborts).
void walk(const std::uint8_t* data, std::size_t size) {
    const ff::Header h = ff::read_header(data);
    ff::PageRecord pages[ff::kMaxPages];
    std::memcpy(pages, data + h.off_pages, h.page_count * sizeof(ff::PageRecord));
    for (std::uint32_t i = 0; i < h.glyph_count; ++i) {
        ff::GlyphRecord g;
        std::memcpy(&g, data + h.off_glyphs + i * sizeof g, sizeof g);
        if (g.page >= h.page_count || g.x + g.w > pages[g.page].width) std::abort();
        if (std::uint32_t{g.kern_first} + g.kern_count > h.kerning_count) std::abort();
    }
    for (int p = 0; p < h.page_count; ++p) {
        std::vector<std::uint8_t> decoded(ff::page_decoded_bytes(pages[p]));
        if (pages[p].offset + pages[p].bytes > size) std::abort();
        const bool ok = pages[p].codec == ff::kCodecRaw
                            ? pages[p].bytes == decoded.size()
                            : ff::packbits_decode(data + pages[p].offset, pages[p].bytes, decoded.data(), decoded.size());
        if (!ok) std::abort();
    }
}

} // namespace

int font_validate_target(const std::uint8_t* data, std::size_t size) {
    if (ff::font_validate(data, size) == nullptr) walk(data, size);
    const auto fixed = with_size_and_crc(data, size);
    if (ff::font_validate(fixed.data(), fixed.size()) == nullptr) walk(fixed.data(), fixed.size());
    return 0;
}

bool font_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    const auto fixed = with_size_and_crc(data, size);
    return ff::font_validate(fixed.data(), fixed.size()) == nullptr;
}

} // namespace nnh::fuzz
