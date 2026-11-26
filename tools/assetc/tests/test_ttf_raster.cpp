// Rasterising outlines (spec 029 T4.7, FR-1, FR-4, FR-6): the rasteriser on the fixtures directly — metrics rounded once
// (em 1000: ascender 800, descender −200, 'A' 560, kern A V −80), coverage in 16 levels, a box glyph's solid inside,
// anti-aliasing off, glyf and CFF giving the same glyphs, kerning from `kern` and GPOS, code points not in the font
// counted, the same bytes twice.

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "test.hpp"
#include "ttf_raster.hpp"

using namespace nnh::assetc;

namespace {

const std::string kDir = NNH_FONTS_DIR;

std::vector<unsigned char> load(const char* name) {
    std::vector<unsigned char> out;
    std::FILE* f = std::fopen((kDir + "/" + name).c_str(), "rb");
    if (f == nullptr) return out;
    int c;
    while ((c = std::fgetc(f)) != EOF) out.push_back(static_cast<unsigned char>(c));
    std::fclose(f);
    return out;
}

struct Raster {
    BakedFont font;
    int skipped = 0;
    char32_t first_skipped = 0;
    std::string problem;
};

Raster bake(const char* file, int size, std::set<char32_t> wanted, bool antialias = true) {
    Raster r;
    const auto data = load(file);
    RasterOptions o;
    o.size = size;
    o.antialias = antialias;
    r.problem = raster_font(data, o, wanted, r.font, r.skipped, r.first_skipped);
    return r;
}

const BakedGlyph* glyph(const BakedFont& f, char32_t cp) {
    for (const BakedGlyph& g : f.glyphs)
        if (g.cp == cp) return &g;
    return nullptr;
}

int kern(const BakedFont& f, char32_t a, char32_t b) {
    for (const KernPx& k : f.kerning)
        if (k.left == a && k.right == b) return k.amount;
    return 0;
}

} // namespace

NNH_TEST(ttf_raster_metrics_are_rounded_once) {
    for (const int size : {8, 12, 16, 32}) {
        const Raster r = bake("nnhtest.ttf", size, {U'A', U'V', U'a'});
        CHECK(r.problem.empty());
        CHECK(r.font.ascent == scale_units(800, size, 1000));
        CHECK(r.font.descent == scale_units(-200, size, 1000));
        const BakedGlyph* a = glyph(r.font, U'A');
        CHECK(a != nullptr && a->advance == scale_units(560, size, 1000));
        CHECK(kern(r.font, U'A', U'V') == scale_units(-80, size, 1000));
    }
}

NNH_TEST(ttf_raster_coverage_has_16_levels_and_a_solid_inside) {
    const Raster r = bake("nnhtest.ttf", 32, {U'a'});
    const BakedGlyph* a = glyph(r.font, U'a');
    CHECK(a != nullptr);
    if (a == nullptr) return;
    int max = 0;
    for (const std::uint8_t p : a->glyph.pixels) {
        CHECK(p <= 15);
        max = p > max ? p : max;
    }
    CHECK(max == 15);
    // 'a' is one filled box (50..390 units wide at 32 px): its middle row is solid
    const int y = a->glyph.h / 2;
    int solid = 0;
    for (int x = 2; x < a->glyph.w - 2; ++x) solid += a->glyph.pixels[static_cast<std::size_t>(y * a->glyph.w + x)] == 15;
    CHECK(solid == a->glyph.w - 4);
}

NNH_TEST(ttf_raster_antialias_off_is_all_or_nothing) {
    const Raster r = bake("nnhtest.ttf", 13, {U'A', U'g', U'?'}, false);
    for (const BakedGlyph& g : r.font.glyphs)
        for (const std::uint8_t p : g.glyph.pixels) CHECK(p == 0 || p == 15);
}

NNH_TEST(ttf_raster_glyf_and_cff_give_the_same_glyphs) {
    const std::set<char32_t> wanted = {U'A', U'b', U'g', U'0', U'?'};
    const Raster t = bake("nnhtest.ttf", 16, wanted), c = bake("nnhtest_cff.otf", 16, wanted);
    CHECK(t.problem.empty() && c.problem.empty());
    CHECK(t.font.glyphs.size() == c.font.glyphs.size());
    for (const BakedGlyph& g : t.font.glyphs) {
        const BakedGlyph* h = glyph(c.font, g.cp);
        CHECK(h != nullptr);
        if (h == nullptr) continue;
        CHECK(g.advance == h->advance && g.glyph.w == h->glyph.w && g.glyph.h == h->glyph.h && g.glyph.ox == h->glyph.ox &&
              g.glyph.oy == h->glyph.oy);
    }
    CHECK(kern(c.font, U'A', U'V') == kern(t.font, U'A', U'V')); // GPOS and kern
}

NNH_TEST(ttf_raster_counts_code_points_the_font_lacks) {
    const Raster r = bake("nnhtest.ttf", 16, {U'A', U'é', U'一', U'€'});
    CHECK(r.problem.empty());
    CHECK(r.font.glyphs.size() == 1);
    CHECK(r.skipped == 3);
    CHECK(r.first_skipped == U'é');
}

NNH_TEST(ttf_raster_ideographs_do_not_kern_and_bakes_are_repeatable) {
    const std::set<char32_t> wanted = {U'A', U'V', 0x65E5, 0x672C, 0x3042};
    const Raster a = bake("nnhtest.ttf", 16, wanted), b = bake("nnhtest.ttf", 16, wanted);
    for (const KernPx& k : a.font.kerning) CHECK(k.left < 0x3000 && k.right < 0x3000);
    CHECK(a.font.glyphs.size() == b.font.glyphs.size());
    for (std::size_t i = 0; i < a.font.glyphs.size() && i < b.font.glyphs.size(); ++i)
        CHECK(a.font.glyphs[i].glyph.pixels == b.font.glyphs[i].glyph.pixels);
}

NNH_TEST(ttf_raster_refuses_a_damaged_file) {
    Raster r;
    std::vector<unsigned char> junk(64, 0x41);
    RasterOptions o;
    r.problem = raster_font(junk, o, {U'A'}, r.font, r.skipped, r.first_skipped);
    CHECK(!r.problem.empty());
}
