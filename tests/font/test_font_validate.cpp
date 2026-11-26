// The .nnhfont validator (spec 029 FR-2, FR-20, X): every refusal reason on a corrupted copy of a hand-built font (the
// CRC fixed after the corruption, so the check under test is the one that fires).

#include <cstring>
#include <vector>

#include "core/crc32.hpp"
#include "font/font_format.hpp"
#include "font_fixture.hpp"
#include "test.hpp"

using namespace nnh::detail::font;
using font_test::FontBuilder;

namespace {

std::vector<std::uint8_t> good() {
    FontBuilder b = FontBuilder::latin();
    b.kern(U'A', U'V', -1).kern(U'A', U'T', -1).kern(U'T', U'o', -2);
    b.fallbacks = {"fonts/b"};
    b.licence = "own";
    return b.build();
}

void put32(std::vector<std::uint8_t>& f, std::size_t at, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) f[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
}

void fix_crc(std::vector<std::uint8_t>& f) {
    put32(f, kCrcOffset, 0);
    put32(f, kCrcOffset, nnh::detail::crc32(f.data(), f.size()));
}

const char* check(std::vector<std::uint8_t> f) {
    fix_crc(f);
    return font_validate(f.data(), f.size());
}

bool refused_for(const char* reason, const char* part) { return reason != nullptr && std::strstr(reason, part) != nullptr; }

Header header_of(const std::vector<std::uint8_t>& f) { return read_header(f.data()); }

GlyphRecord* glyph(std::vector<std::uint8_t>& f, int i) {
    return reinterpret_cast<GlyphRecord*>(f.data() + header_of(f).off_glyphs + static_cast<std::size_t>(i) * 16);
}

/// The glyph index of `cp` (the records are sorted by code point).
int index_of(const std::vector<std::uint8_t>& f, char32_t cp) {
    const Header h = header_of(f);
    for (std::uint32_t i = 0; i < h.glyph_count; ++i) {
        std::uint32_t c;
        std::memcpy(&c, f.data() + h.off_codepoints + i * 4, 4);
        if (c == cp) return static_cast<int>(i);
    }
    return -1;
}

} // namespace

NNH_TEST(font_validate_accepts_a_written_font) {
    const auto f = good();
    CHECK(font_validate(f.data(), f.size()) == nullptr);
    const Header h = header_of(f);
    CHECK(font_validate_tables(f.data(), h.off_page_data, f.size()) == nullptr);
}

NNH_TEST(font_validate_refuses_short_or_foreign_files) {
    const auto f = good();
    CHECK(refused_for(font_validate(f.data(), 10), "too small"));
    CHECK(refused_for(font_validate(nullptr, 0), "too small"));
    auto bad = f;
    bad[0] = 'X';
    CHECK(refused_for(check(bad), "not a font"));
    bad = f;
    bad[4] = 2;
    CHECK(refused_for(check(bad), "version"));
    bad = f;
    bad[6] = 0x80;
    CHECK(refused_for(check(bad), "flags"));
}

NNH_TEST(font_validate_refuses_a_wrong_size_or_crc) {
    auto f = good();
    auto longer = f;
    longer.push_back(0);
    CHECK(refused_for(check(longer), "file size"));
    f[f.size() - 1] ^= 1;
    CHECK(refused_for(font_validate(f.data(), f.size()), "CRC"));
}

NNH_TEST(font_validate_refuses_bad_metrics_and_counts) {
    auto f = good();
    f[8] = 0; // size 0
    f[9] = 0;
    CHECK(refused_for(check(f), "metrics"));
    f = good();
    put32(f, 16, 0); // no glyphs
    CHECK(refused_for(check(f), "glyph"));
    f = good();
    f[24] = 9; // pages
    CHECK(refused_for(check(f), "pages"));
    f = good();
    f[25] = 4; // fallbacks
    CHECK(refused_for(check(f), "fallback"));
}

NNH_TEST(font_validate_refuses_sections_out_of_order) {
    auto f = good();
    put32(f, 48, header_of(f).off_glyphs + 4); // the glyph records moved
    CHECK(refused_for(check(f), "layout"));
}

NNH_TEST(font_validate_refuses_code_points_not_ascending_or_too_large) {
    auto f = good();
    const std::size_t cps = header_of(f).off_codepoints;
    put32(f, cps + 4, static_cast<std::uint32_t>(U' ')); // equal to its neighbour: ' ' is first
    CHECK(refused_for(check(f), "ascending"));
    f = good();
    const std::uint32_t n = header_of(f).glyph_count;
    put32(f, cps + (n - 1) * 4, 0x110000);
    CHECK(refused_for(check(f), "code point"));
}

NNH_TEST(font_validate_refuses_records_outside_their_page) {
    auto f = good();
    glyph(f, index_of(f, U'A'))->x = 125; // 6 wide on a 128-wide page
    CHECK(refused_for(check(f), "outside its page"));
    f = good();
    glyph(f, index_of(f, U'A'))->y = 60; // 9 high on a 64-high page
    CHECK(refused_for(check(f), "outside its page"));
    f = good();
    glyph(f, 1)->page = 1;
    CHECK(refused_for(check(f), "page"));
}

NNH_TEST(font_validate_refuses_bad_kerning_runs) {
    auto f = good();
    const Header h = header_of(f);
    int a = -1;
    for (std::uint32_t i = 0; i < h.glyph_count; ++i)
        if (glyph(f, static_cast<int>(i))->kern_count == 2) a = static_cast<int>(i);
    CHECK(a >= 0);
    if (a < 0) return;
    auto bad = f;
    glyph(bad, a)->kern_count = 9;
    CHECK(refused_for(check(bad), "kerning"));
    bad = f;
    auto* pairs = reinterpret_cast<KernPair*>(bad.data() + h.off_kerning + glyph(bad, a)->kern_first * 4u);
    const KernPair t = pairs[0];
    pairs[0] = pairs[1];
    pairs[1] = t;
    CHECK(refused_for(check(bad), "sorted"));
    bad = f;
    glyph(bad, a)->flags &= static_cast<std::uint8_t>(~kGlyphKerning);
    CHECK(refused_for(check(bad), "kerning"));
    bad = f;
    pairs = reinterpret_cast<KernPair*>(bad.data() + h.off_kerning);
    pairs[0].right = 5000;
    CHECK(refused_for(check(bad), "kerning"));
}

NNH_TEST(font_validate_refuses_unterminated_paths_and_bad_glyph_indexes) {
    auto f = good();
    std::memset(f.data() + header_of(f).off_fallbacks, 'a', 64);
    CHECK(refused_for(check(f), "path"));
    f = good();
    put32(f, 28, 9999); // replacement
    CHECK(refused_for(check(f), "replacement"));
    f = good();
    put32(f, 32, 9999); // ellipsis
    CHECK(refused_for(check(f), "ellipsis"));
}

NNH_TEST(font_validate_refuses_bad_pages) {
    auto f = good();
    const std::size_t page = header_of(f).off_pages;
    auto bad = f;
    bad[page + 2] = 100; // width 100: not a power of two
    CHECK(refused_for(check(bad), "side"));
    bad = f;
    bad[page] = 7;
    CHECK(refused_for(check(bad), "format"));
    bad = f;
    bad[page + 1] = 9;
    CHECK(refused_for(check(bad), "codec"));
    bad = f;
    bad[page + 1] = bad[page + 1] == kCodecRaw ? kCodecPackBits : kCodecRaw; // the other codec: wrong size
    CHECK(refused_for(check(bad), "decode"));
}

NNH_TEST(font_validate_never_reads_outside_any_truncation) {
    const auto f = good();
    for (std::size_t n = 0; n < f.size(); n += 7) {
        std::vector<std::uint8_t> cut(f.begin(), f.begin() + static_cast<long>(n));
        CHECK(font_validate(cut.data(), cut.size()) != nullptr);
    }
}
