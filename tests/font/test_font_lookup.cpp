// The font block's lookups (spec 029 FR-6, FR-34; plan decision 6): a Latin-1 table, BMP buckets of 32 code points
// (binary search inside, a dense bucket read directly), a binary search beyond the BMP, kerning runs per left glyph.

#include <vector>

#include "font/font_block.hpp"
#include "font_fixture.hpp"
#include "test.hpp"

using namespace nnh::detail;
using font_test::FontBuilder;

namespace {

/// A block made from `f` (validated tables) in `storage`.
const FontBlock* block_of(const std::vector<std::uint8_t>& f, std::vector<std::uint64_t>& storage) {
    const font::Header h = font::read_header(f.data());
    storage.assign(font_block_bytes(h) / 8 + 1, 0);
    return font_block_init(storage.data(), font_block_bytes(h), f.data(), h.off_page_data);
}

} // namespace

NNH_TEST(font_lookup_latin1_through_the_table) {
    FontBuilder b;
    b.glyph(U' ', 0, 0, 0, 0, 0, 0, 3).glyph(U'A', 0, 0, 5, 5, 0, 0, 6).glyph(0xFF, 8, 0, 5, 5, 0, 0, 6);
    b.glyph(0x100, 16, 0, 5, 5, 0, 0, 6);
    std::vector<std::uint64_t> mem;
    const FontBlock* f = block_of(b.build(), mem);
    CHECK(f != nullptr);
    if (f == nullptr) return;
    CHECK(find_glyph(*f, U' ') == 0 && find_glyph(*f, U'A') == 1 && find_glyph(*f, 0xFF) == 2);
    CHECK(find_glyph(*f, 0x100) == 3);
    CHECK(find_glyph(*f, U'B') == -1 && find_glyph(*f, 0) == -1 && find_glyph(*f, 0xFE) == -1);
}

NNH_TEST(font_lookup_buckets_with_0_to_32_glyphs_and_their_edges) {
    FontBuilder b;
    int x = 0;
    auto add = [&](char32_t cp) { b.glyph(cp, (x++ % 8) * 8, 0, 1, 1, 0, 0, 2); };
    for (char32_t cp = 0x4E00; cp < 0x4E20; ++cp) add(cp);   // bucket 0x270: all 32
    for (char32_t cp = 0x4E20; cp < 0x4E40; cp += 31) add(cp); // bucket 0x271: its first and last
    add(0x4E5F);                                               // bucket 0x272: one, the last code point
    add(0xFFFF);                                               // the last BMP code point
    std::vector<std::uint64_t> mem;
    const FontBlock* f = block_of(b.build(), mem);
    CHECK(f != nullptr);
    if (f == nullptr) return;
    bool ok = true;
    for (char32_t cp = 0x4E00; cp < 0x4E20; ++cp) ok = ok && find_glyph(*f, cp) == static_cast<int>(cp - 0x4E00);
    CHECK(ok);
    CHECK(find_glyph(*f, 0x4E20) == 32 && find_glyph(*f, 0x4E3F) == 33 && find_glyph(*f, 0x4E21) == -1);
    CHECK(find_glyph(*f, 0x4E40) == -1 && find_glyph(*f, 0x4E5F) == 34 && find_glyph(*f, 0x4E5E) == -1);
    CHECK(find_glyph(*f, 0xFFFF) == 35 && find_glyph(*f, 0xFFFE) == -1 && find_glyph(*f, 0x4DFF) == -1);
}

NNH_TEST(font_lookup_beyond_the_bmp) {
    FontBuilder b;
    b.glyph(U'a', 0, 0, 1, 1, 0, 0, 2).glyph(0x10000, 8, 0, 1, 1, 0, 0, 2).glyph(0x20B9F, 16, 0, 1, 1, 0, 0, 2);
    b.glyph(0x10FFFF, 24, 0, 1, 1, 0, 0, 2);
    std::vector<std::uint64_t> mem;
    const FontBlock* f = block_of(b.build(), mem);
    CHECK(f != nullptr);
    if (f == nullptr) return;
    CHECK(find_glyph(*f, 0x10000) == 1 && find_glyph(*f, 0x20B9F) == 2 && find_glyph(*f, 0x10FFFF) == 3);
    CHECK(find_glyph(*f, 0x10001) == -1 && find_glyph(*f, 0x110000) == -1 && find_glyph(*f, 0x20B9E) == -1);
}

NNH_TEST(font_lookup_a_latin1_font_has_no_buckets) {
    FontBuilder latin;
    latin.glyph(U'A', 0, 0, 5, 5, 0, 0, 6).glyph(0xE9, 8, 0, 5, 5, 0, 0, 6);
    FontBuilder more = latin;
    more.glyph(0x4E00, 16, 0, 5, 5, 0, 0, 6);
    const auto f = latin.build(), g = more.build();
    CHECK((font::read_header(f.data()).flags & font::kFlagBmp) == 0);
    CHECK((font::read_header(g.data()).flags & font::kFlagBmp) != 0);
    CHECK(font_block_bytes(font::read_header(g.data())) >= font_block_bytes(font::read_header(f.data())) + 4096);
    std::vector<std::uint64_t> mem;
    const FontBlock* block = block_of(f, mem);
    CHECK(block != nullptr && block->buckets == nullptr && find_glyph(*block, 0x4E00) == -1 && find_glyph(*block, 0xE9) == 1);
}

NNH_TEST(font_lookup_kerning_runs_of_0_1_and_255_pairs) {
    FontBuilder b;
    for (int i = 0; i < 256; ++i) b.glyph(0x4E00 + static_cast<char32_t>(i), (i % 8) * 8, (i / 8) % 8 * 8, 1, 1, 0, 0, 2);
    b.kern(0x4E00, 0x4E01, -3);
    for (int i = 1; i < 256; ++i) b.kern(0x4E02, 0x4E00 + static_cast<char32_t>(i), i % 2 == 0 ? 1 : -1);
    std::vector<std::uint64_t> mem;
    const FontBlock* f = block_of(b.build(), mem);
    CHECK(f != nullptr);
    if (f == nullptr) return;
    CHECK(kerning(*f, 0, 1) == -3 && kerning(*f, 0, 2) == 0 && kerning(*f, 1, 0) == 0); // 1 pair; 0 pairs
    bool ok = true;
    for (int i = 1; i < 256; ++i) ok = ok && kerning(*f, 2, i) == (i % 2 == 0 ? 1 : -1);
    CHECK(ok);
    CHECK(kerning(*f, 2, 0) == 0);
}

NNH_TEST(font_lookup_at_most_3_compares_on_a_cjk_table) {
    FontBuilder b;
    int n = 0;
    for (char32_t cp = 0x4E00; cp <= 0x9FFF; cp += 7) b.glyph(cp, (n++ % 64) * 2, 0, 1, 1, 0, 0, 2); // ≈ 3000 kanji
    b.page_side = 128;
    std::vector<std::uint64_t> mem;
    const FontBlock* f = block_of(b.build(), mem);
    CHECK(f != nullptr);
    if (f == nullptr) return;
    int worst = 0;
    bool found = true;
    for (char32_t cp = 0x4E00; cp <= 0x9FFF; ++cp) {
        int compares = 0;
        const int g = find_glyph_counted(*f, cp, compares);
        found = found && ((cp - 0x4E00) % 7 == 0 ? g == static_cast<int>((cp - 0x4E00) / 7) : g == -1);
        if (compares > worst) worst = compares;
    }
    CHECK(found);
    CHECK(worst <= 3);
}
