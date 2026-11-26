// Packing glyphs into pages (spec 029 T4.8, FR-2): deterministic, the smallest page that holds them, padding between
// glyphs, a forced side, more pages when one is full, the 8-page refusal; the outline baking.

#include <set>
#include <string>
#include <vector>

#include "glyph_pack.hpp"
#include "test.hpp"

using namespace nnh::assetc;

namespace {

BakedFont boxes(int count, int w, int h, int padding = 1) {
    BakedFont f;
    f.size = h;
    f.padding = padding;
    for (int i = 0; i < count; ++i) {
        BakedGlyph g;
        g.cp = static_cast<char32_t>(0x4E00 + i);
        g.advance = w;
        g.glyph.w = w - (i % 3);
        g.glyph.h = h - (i % 2);
        g.glyph.pixels.assign(static_cast<std::size_t>(g.glyph.w * g.glyph.h), 15);
        f.glyphs.push_back(g);
    }
    return f;
}

bool overlap(const BakedGlyph& a, const BakedGlyph& b, int pad) {
    if (a.page != b.page) return false;
    return a.x < b.x + b.glyph.w + pad && b.x < a.x + a.glyph.w + pad && a.y < b.y + b.glyph.h + pad &&
           b.y < a.y + a.glyph.h + pad;
}

} // namespace

NNH_TEST(glyph_pack_places_without_overlap_inside_the_smallest_page) {
    BakedFont f = boxes(60, 12, 14);
    std::vector<PageSize> pages;
    CHECK(pack_glyphs(f, pages).empty());
    CHECK(pages.size() == 1);
    if (pages.empty()) return;
    CHECK(pages[0].w >= 64 && pages[0].w <= 256 && (pages[0].w & (pages[0].w - 1)) == 0);
    int bad = 0;
    for (std::size_t i = 0; i < f.glyphs.size(); ++i) {
        const BakedGlyph& g = f.glyphs[i];
        bad += g.x < 0 || g.y < 0 || g.x + g.glyph.w > pages[0].w || g.y + g.glyph.h > pages[0].h ? 1 : 0;
        for (std::size_t j = i + 1; j < f.glyphs.size(); ++j) bad += overlap(g, f.glyphs[j], 1) ? 1 : 0;
    }
    CHECK(bad == 0);
    // a smaller page couldn't hold the same glyphs
    CHECK(static_cast<long>(pages[0].w) * pages[0].h >= 60L * 13 * 14);
}

NNH_TEST(glyph_pack_is_deterministic) {
    BakedFont a = boxes(300, 13, 15), b = boxes(300, 13, 15);
    std::vector<PageSize> pa, pb;
    CHECK(pack_glyphs(a, pa).empty() && pack_glyphs(b, pb).empty());
    CHECK(pa.size() == pb.size());
    bool same = true;
    for (std::size_t i = 0; i < a.glyphs.size(); ++i)
        same = same && a.glyphs[i].page == b.glyphs[i].page && a.glyphs[i].x == b.glyphs[i].x && a.glyphs[i].y == b.glyphs[i].y;
    CHECK(same);
}

NNH_TEST(glyph_pack_forced_side_and_more_pages) {
    BakedFont f = boxes(400, 30, 30);
    f.page_side = 256;
    std::vector<PageSize> pages;
    CHECK(pack_glyphs(f, pages).empty());
    CHECK(pages.size() > 1);
    for (const PageSize& p : pages) CHECK(p.w == 256 && p.h == 256);
    std::set<int> used;
    for (const BakedGlyph& g : f.glyphs) used.insert(g.page);
    CHECK(static_cast<int>(used.size()) == static_cast<int>(pages.size()));
}

NNH_TEST(glyph_pack_refuses_more_than_8_pages) {
    BakedFont f = boxes(9000, 100, 100);
    std::vector<PageSize> pages;
    const std::string why = pack_glyphs(f, pages);
    CHECK(why.find("glyphs at 100 px need") != std::string::npos);
    CHECK(why.find("pages of 1024x1024 (limit 8): reduce the size or the character set") != std::string::npos);
}

NNH_TEST(glyph_pack_padding_is_kept) {
    BakedFont f = boxes(40, 10, 10, 3);
    std::vector<PageSize> pages;
    CHECK(pack_glyphs(f, pages).empty());
    int bad = 0;
    for (std::size_t i = 0; i < f.glyphs.size(); ++i)
        for (std::size_t j = i + 1; j < f.glyphs.size(); ++j) bad += overlap(f.glyphs[i], f.glyphs[j], 3) ? 1 : 0;
    CHECK(bad == 0);
}

NNH_TEST(glyph_pack_outline_is_the_disk_dilation) {
    Bitmap dot;
    dot.w = dot.h = 1;
    dot.ox = 5;
    dot.oy = 7;
    dot.pixels = {15};
    const Bitmap o = bake_outline(dot, 2);
    CHECK(o.w == 5 && o.h == 5 && o.ox == 3 && o.oy == 5);
    // corners are outside the disk (dx² + dy² = 8 > r² + r = 6), the centre's row and column are inside
    CHECK(o.pixels.size() == 25);
    if (o.pixels.size() == 25) {
        CHECK(o.pixels[0] == 0 && o.pixels[4] == 0 && o.pixels[20] == 0 && o.pixels[24] == 0);
        CHECK(o.pixels[2] == 15 && o.pixels[10] == 15 && o.pixels[12] == 15 && o.pixels[14] == 15 && o.pixels[22] == 15);
    }
}
