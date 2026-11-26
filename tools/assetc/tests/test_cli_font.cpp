// The font converter end to end (spec 029 FR-1..FR-4, FR-9, AC-1): the generated fixtures (tests/data/fonts, made by
// scripts/make_test_fonts.py) converted, read back with the engine's own validator, metrics against hand-computed values
// (em 1000, ascender 800, descender −200; 'A' advances 560 units, 'a' 440; kern A V −80), BMFont text and XML forms,
// colour pages, outlines, refusals.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "cli.hpp"
#include "cli_fixture.hpp"
#include "convert_font.hpp"
#include "files.hpp"
#include "paths.hpp"
#include "font/font_format.hpp"
#include "test.hpp"

using namespace nnh::assetc;
namespace ff = nnh::detail::font;

namespace {

const std::string kDir = NNH_FONTS_DIR;

struct Converted {
    std::string problems;
    FontOutput out;
    ff::Header h{};
};

Converted convert(const char* file) {
    Converted c;
    DiskFiles disk;
    FontInput in;
    in.path = kDir + "/" + file;
    in.assets = kDir;
    in.shown = std::string("fonts/") + file;
    c.problems = convert_font(disk, in, c.out);
    if (c.out.bytes.size() >= ff::kHeaderSize) c.h = ff::read_header(c.out.bytes.data());
    return c;
}

bool valid(const Converted& c) {
    return c.problems.empty() && ff::font_validate(c.out.bytes.data(), c.out.bytes.size()) == nullptr;
}

/// The record of `cp`, or a default one.
ff::GlyphRecord record(const Converted& c, char32_t cp, int& index) {
    ff::GlyphRecord r;
    index = -1;
    for (std::uint32_t i = 0; i < c.h.glyph_count; ++i) {
        std::uint32_t v;
        std::memcpy(&v, c.out.bytes.data() + c.h.off_codepoints + i * 4, 4);
        if (v != cp) continue;
        std::memcpy(&r, c.out.bytes.data() + c.h.off_glyphs + i * 16, 16);
        index = static_cast<int>(i);
    }
    return r;
}

} // namespace

NNH_TEST(cli_font_truetype_metrics_are_rounded_once) {
    const Converted c = convert("body_16.font");
    CHECK(valid(c));
    CHECK(c.h.size == 16 && c.h.ascent == 13 && c.h.descent == -3 && c.h.line_height == 16); // 12.8, −3.2
    int a = 0, v = 0;
    const ff::GlyphRecord ra = record(c, U'A', a);
    record(c, U'V', v);
    CHECK(a >= 0 && ra.advance == 9 && ra.w == 8 && ra.h == 12 && ra.ox == 0 && ra.oy == 1); // 8.96; box 0..8 × −12..0
    CHECK((ra.flags & ff::kGlyphKerning) != 0 && ra.kern_count == 1);
    ff::KernPair k;
    std::memcpy(&k, c.out.bytes.data() + c.h.off_kerning + ra.kern_first * 4u, 4);
    CHECK(k.right == v && k.amount == -1); // −80 × 16 / 1000 = −1.28
    CHECK(c.h.ellipsis != ff::kNoGlyph && c.h.replacement != ff::kNoGlyph); // U+2026 and U+FFFD in the set
    CHECK(c.out.report.rfind("font fonts/body_16.font: ", 0) == 0 && c.out.report.find("licence own") != std::string::npos);
}

NNH_TEST(cli_font_sizes_8_12_32) {
    const Converted s8 = convert("body_8.font"), s12 = convert("body_12.font"), s32 = convert("body_32.font");
    CHECK(valid(s8) && valid(s12) && valid(s32));
    CHECK(s8.h.ascent == 6 && s8.h.descent == -2 && s8.h.line_height == 8);
    CHECK(s12.h.ascent == 10 && s12.h.descent == -2 && s12.h.line_height == 12);
    CHECK(s32.h.ascent == 26 && s32.h.descent == -6 && s32.h.line_height == 32);
    int i = 0;
    CHECK(record(s32, U'a', i).advance == 14); // 440 × 32 / 1000 = 14.08
}

NNH_TEST(cli_font_cff_outlines_and_gpos_kerning) {
    const Converted c = convert("cff_16.font");
    CHECK(valid(c));
    int a = 0, t = 0, o = 0;
    const ff::GlyphRecord ra = record(c, U'A', a);
    const ff::GlyphRecord rt = record(c, U'T', t);
    record(c, U'o', o);
    CHECK(ra.advance == 9 && ra.w == 8 && ra.h == 12); // the same box as the glyf fixture's
    CHECK((rt.flags & ff::kGlyphKerning) != 0);
    ff::KernPair k;
    std::memcpy(&k, c.out.bytes.data() + c.h.off_kerning + rt.kern_first * 4u, 4);
    CHECK(k.right == o && k.amount == -1); // −60 × 16 / 1000 = −0.96
}

NNH_TEST(cli_font_antialias_off_is_full_coverage_or_nothing) {
    const Converted c = convert("pixel_12.font");
    CHECK(valid(c) && (c.h.flags & ff::kFlagAntialias) == 0);
}

NNH_TEST(cli_font_outlines_are_baked_around_each_glyph) {
    const Converted one = convert("outline_16.font"), three = convert("outline3_16.font");
    CHECK(valid(one) && valid(three) && (one.h.flags & ff::kFlagOutline) != 0);
    int a = 0;
    const ff::GlyphRecord ra = record(three, U'A', a);
    ff::OutlineRecord o;
    std::memcpy(&o, three.out.bytes.data() + three.h.off_outlines + static_cast<std::size_t>(a) * 8, 8);
    CHECK(o.w == ra.w + 6 && o.h == ra.h + 6 && o.ox == ra.ox - 3 && o.oy == ra.oy - 3);
}

NNH_TEST(cli_font_bmfont_text_and_xml_give_the_same_glyphs) {
    const Converted t = convert("mono.fnt"), x = convert("mono_xml.fnt");
    CHECK(valid(t) && valid(x));
    CHECK(t.h.line_height == 10 && t.h.ascent == 8 && t.h.descent == -2 && t.h.glyph_count == 37);
    CHECK(x.h.glyph_count == t.h.glyph_count && (t.h.flags & ff::kFlagColour) == 0);
    int a = 0;
    const ff::GlyphRecord r = record(t, U'A', a);
    CHECK(r.w == 7 && r.h == 8 && r.ox == 0 && r.oy == 1 && r.advance == 8);
}

NNH_TEST(cli_font_bmfont_colour_pages_stay_rgba) {
    const Converted c = convert("colour.fnt");
    CHECK(valid(c) && (c.h.flags & ff::kFlagColour) != 0);
}

NNH_TEST(cli_font_refuses_with_file_and_reason) {
    MemoryFiles mem;
    mem.add("a/x.font", "source = \"missing.ttf\"\nsize = 12\nlicence = \"own\"\n");
    mem.add("a/y.font", "source = \"y.ttf\"\nsize = 12\nlicence = \"licences/none.txt\"\n");
    mem.add("a/y.ttf", "not a font at all");
    mem.add("a/z.fnt", "BMF\x03 binary");
    FontInput in;
    in.assets = "a";
    FontOutput out;
    in.path = "a/x.font";
    CHECK(convert_font(mem, in, out) == "a/x.font:1: missing.ttf can't be read (or is larger than 32 MiB)");
    in.path = "a/y.font";
    CHECK(convert_font(mem, in, out).find("a/y.font: y.ttf: not a TrueType or OpenType font") == 0);
    in.path = "a/z.fnt";
    CHECK(convert_font(mem, in, out) == "a/z.fnt:1: binary BMFont files aren't read: export the text or XML form\n");
}

NNH_TEST(cli_font_depfile_paths_are_relative_to_the_working_directory) {
    // review S5: absolute "E:/..." paths made devkitPro's MSYS CMake/Ninja read every font input as a missing relative
    // file, so each font reconverted on every 3DS build (a scene edit took 6.8 s)
    const std::string out = std::string(NNH_ASSETC_TEST_TMP) + "/font_dep.nnhfont";
    const std::string dep = out + ".d";
    std::remove(dep.c_str());
    const std::string in = kDir + "/body_16.font";
    const cli_test::Run r =
        cli_test::run({"font", in, out, "--assets", kDir, "--show", "fonts/body_16", "--depfile", dep});
    CHECK(r.code == 0);
    std::string text;
    CHECK(DiskFiles{}.read(dep, 1 << 16, text));
    const std::string cwd = std::filesystem::current_path().generic_string();
    CHECK(text.rfind(relative_path(cwd, out) + ": " + relative_path(cwd, in) + " ", 0) == 0);
    CHECK(text.find(':') == text.find(": ")); // no drive letter left
}
