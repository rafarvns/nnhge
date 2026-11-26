// The .nnhfont format (spec 029 FR-2, D49; plan "The .nnhfont format"): a hand-built font written by the shared writer
// and read back field by field; sections in order without gaps; pages stored with PackBits when smaller.

#include <cstring>

#include "core/crc32.hpp"
#include "font/font_format.hpp"
#include "font/page_codec.hpp"
#include "font_fixture.hpp"
#include "test.hpp"

using namespace nnh::detail::font;
using font_test::FontBuilder;

namespace {

std::uint32_t u32_at(const std::vector<std::uint8_t>& f, std::size_t at) {
    return static_cast<std::uint32_t>(f[at] | (f[at + 1] << 8) | (f[at + 2] << 16)) |
           (static_cast<std::uint32_t>(f[at + 3]) << 24);
}

FontBuilder small() {
    FontBuilder b;
    b.size = 12;
    b.line_height = 15;
    b.ascent = 11;
    b.descent = -3;
    b.page_side = 32;
    b.page_height = 16;
    b.glyph(U'B', 8, 0, 6, 9, 1, 2, 7).glyph(U'A', 0, 0, 7, 9, 0, 2, 8).glyph(0x20AC, 16, 0, 8, 10, 0, 1, 9);
    b.kern(U'A', U'B', -1).kern(U'A', 0x20AC, 2);
    b.fallbacks = {"fonts/cjk_12"};
    b.licence = "licences/OFL.txt";
    return b;
}

} // namespace

NNH_TEST(font_format_header_reads_back) {
    const auto f = small().build();
    CHECK(f.size() > kHeaderSize);
    const Header h = read_header(f.data());
    CHECK(h.magic == kMagic && h.version == 1);
    CHECK(h.size == 12 && h.line_height == 15 && h.ascent == 11 && h.descent == -3);
    CHECK(h.glyph_count == 3 && h.kerning_count == 2 && h.page_count == 1 && h.fallback_count == 1);
    CHECK(h.flags == (kFlagKerning | kFlagBmp)); // € is above U+00FF
    CHECK(h.file_size == f.size());
    CHECK(h.replacement == kNoGlyph && h.ellipsis == kNoGlyph);
}

NNH_TEST(font_format_sections_follow_each_other_without_gaps) {
    const auto f = small().build();
    const Header h = read_header(f.data());
    CHECK(h.off_codepoints == 96);
    CHECK(h.off_glyphs == 96 + 3 * 4);
    CHECK(h.off_outlines == 0);
    CHECK(h.off_kerning == h.off_glyphs + 3 * 16);
    CHECK(h.off_fallbacks == h.off_kerning + 2 * 4);
    CHECK(h.off_licence == h.off_fallbacks + 64);
    CHECK(h.off_pages == h.off_licence + 64);
    CHECK(h.off_page_data == h.off_pages + 16);
}

NNH_TEST(font_format_tables_hold_the_sorted_glyphs_and_kerning_runs) {
    const auto f = small().build();
    const Header h = read_header(f.data());
    CHECK(u32_at(f, h.off_codepoints) == U'A' && u32_at(f, h.off_codepoints + 4) == U'B');
    CHECK(u32_at(f, h.off_codepoints + 8) == 0x20AC);
    GlyphRecord a, b;
    std::memcpy(&a, f.data() + h.off_glyphs, 16);
    std::memcpy(&b, f.data() + h.off_glyphs + 16, 16);
    CHECK(a.x == 0 && a.w == 7 && a.h == 9 && a.ox == 0 && a.oy == 2 && a.advance == 8);
    CHECK(a.kern_count == 2 && a.kern_first == 0 && (a.flags & kGlyphKerning) != 0);
    CHECK(b.x == 8 && b.kern_count == 0 && (b.flags & kGlyphKerning) == 0);
    KernPair k0, k1;
    std::memcpy(&k0, f.data() + h.off_kerning, 4);
    std::memcpy(&k1, f.data() + h.off_kerning + 4, 4);
    CHECK(k0.right == 1 && k0.amount == -1 && k1.right == 2 && k1.amount == 2);
    CHECK(std::strcmp(reinterpret_cast<const char*>(f.data() + h.off_fallbacks), "fonts/cjk_12") == 0);
    CHECK(std::strcmp(reinterpret_cast<const char*>(f.data() + h.off_licence), "licences/OFL.txt") == 0);
}

NNH_TEST(font_format_a4_page_is_packed_then_packbits_when_smaller) {
    const auto f = small().build();
    const Header h = read_header(f.data());
    PageRecord p;
    std::memcpy(&p, f.data() + h.off_pages, 16);
    CHECK(p.format == kPageA4 && p.width == 32 && p.height == 16);
    CHECK(page_decoded_bytes(p) == 32 * 16 / 2);
    CHECK(p.offset == h.off_page_data && p.offset + p.bytes == f.size());
    CHECK(p.codec == kCodecPackBits || p.bytes == 256);
    std::uint8_t packed[256];
    const bool decoded = p.codec == kCodecRaw ? (std::memcpy(packed, f.data() + p.offset, 256), true)
                                              : packbits_decode(f.data() + p.offset, p.bytes, packed, 256);
    CHECK(decoded);
    std::uint8_t coverage[512];
    a4_unpack(packed, 512, coverage);
    CHECK(coverage[0] == (1 + (0 * 3 + 0) % 15) * 17); // 'A' at (0, 0)
    CHECK(coverage[31] == 0);                          // nothing at (31, 0)
}

NNH_TEST(font_format_crc_covers_the_file_with_its_field_zero) {
    auto f = small().build();
    const std::uint32_t stored = read_header(f.data()).crc;
    for (int i = 0; i < 4; ++i) f[kCrcOffset + static_cast<std::size_t>(i)] = 0;
    CHECK(stored == nnh::detail::crc32(f.data(), f.size()));
}

NNH_TEST(font_format_outline_records_follow_the_glyphs) {
    FontBuilder b = small();
    b.glyphs[0].has_outline = false;
    b.glyph(U'C', 0, 10, 4, 4, 0, 0, 5).outline(8, 10, 6, 6, -1, -1);
    const auto f = b.build();
    const Header h = read_header(f.data());
    CHECK((h.flags & kFlagOutline) != 0);
    CHECK(h.off_outlines == h.off_glyphs + 4 * 16);
    CHECK(h.off_kerning == h.off_outlines + 4 * 8);
    OutlineRecord o;
    std::memcpy(&o, f.data() + h.off_outlines + 2 * 8, 8); // A, B, C, €: C is index 2
    CHECK(o.xp == 8 && o.y == 10 && o.w == 6 && o.ox == -1);
}
