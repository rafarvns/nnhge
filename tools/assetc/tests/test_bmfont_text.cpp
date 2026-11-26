// Reading BMFont files (spec 029 T4.3, FR-1, X): the text and XML forms through one tokenizer, every field, quoting,
// refusals with their line — the binary form, packed channels, values out of range, a character twice, an unclosed
// quote, a missing "common" line, size and line limits.

#include <string>

#include "bmfont_text.hpp"
#include "test.hpp"

using namespace nnh::assetc;

namespace {

std::string problems_of(const std::string& text, BmFont* out = nullptr) {
    BmFont f;
    Diagnostics p;
    read_bmfont(text, out != nullptr ? *out : f, p);
    return p.text("fonts/a.fnt");
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

const char* kText = "info face=\"Pixel Font\" size=-8 bold=0\n"
                    "common lineHeight=10 base=8 scaleW=64 scaleH=64 pages=2 packed=0\n"
                    "page id=0 file=\"a_0.png\"\npage id=1 file=\"a_1.png\"\n"
                    "chars count=2\n"
                    "char id=65 x=1 y=2 width=7 height=8 xoffset=-1 yoffset=1 xadvance=8 page=0 chnl=15\n"
                    "char id=12354 x=10 y=0 width=9 height=9 xoffset=0 yoffset=0 xadvance=10 page=1 chnl=15\n"
                    "kernings count=1\nkerning first=65 second=86 amount=-2\n";

} // namespace

NNH_TEST(bmfont_text_form_every_field) {
    BmFont f;
    Diagnostics p;
    CHECK(read_bmfont(kText, f, p));
    CHECK(p.empty());
    CHECK(f.size == 8 && f.line_height == 10 && f.base == 8 && f.pages == 2 && !f.packed);
    CHECK(f.page_files.size() == 2 && f.page_files[0] == "a_0.png" && f.page_files[1] == "a_1.png");
    CHECK(f.chars.size() == 2);
    if (f.chars.size() == 2) {
        const BmChar& a = f.chars[0];
        CHECK(a.id == 65 && a.x == 1 && a.y == 2 && a.w == 7 && a.h == 8 && a.xoffset == -1 && a.yoffset == 1);
        CHECK(a.xadvance == 8 && a.page == 0 && a.line == 6);
        CHECK(f.chars[1].id == 12354 && f.chars[1].page == 1);
    }
    CHECK(f.kernings.size() == 1 && f.kernings[0].first == 65 && f.kernings[0].second == 86 && f.kernings[0].amount == -2);
}

NNH_TEST(bmfont_xml_form_reads_the_same) {
    const char* xml = "<?xml version=\"1.0\"?>\n<font>\n"
                      "  <info face=\"Pixel Font\" size=\"8\"/>\n"
                      "  <common lineHeight=\"10\" base=\"8\" scaleW=\"64\" scaleH=\"64\" pages=\"1\" packed=\"0\"/>\n"
                      "  <pages>\n    <page id=\"0\" file=\"a_0.png\" />\n  </pages>\n"
                      "  <chars count=\"1\">\n"
                      "    <char id=\"65\" x=\"1\" y=\"2\" width=\"7\" height=\"8\" xoffset=\"-1\" yoffset=\"1\" "
                      "xadvance=\"8\" page=\"0\" chnl=\"15\" />\n"
                      "  </chars>\n  <kernings count=\"1\">\n    <kerning first=\"65\" second=\"86\" amount=\"-2\" />\n"
                      "  </kernings>\n</font>\n";
    BmFont f;
    Diagnostics p;
    CHECK(read_bmfont(xml, f, p));
    CHECK(p.empty());
    CHECK(f.size == 8 && f.line_height == 10 && f.pages == 1 && f.page_files.size() == 1 && f.page_files[0] == "a_0.png");
    CHECK(f.chars.size() == 1 && f.chars[0].xoffset == -1 && f.chars[0].xadvance == 8);
    CHECK(f.kernings.size() == 1 && f.kernings[0].amount == -2);
}

NNH_TEST(bmfont_refusals_name_their_line) {
    CHECK(has(problems_of("BMF\x03rest"), "fonts/a.fnt:1: binary BMFont files aren't read: export the text or XML form"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1 packed=1\n"),
              "fonts/a.fnt:1: packed BMFont channels aren't read: export one glyph per texel"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\nchar id=65 x=1 width=300\n"),
              "fonts/a.fnt:2: width is 0 to 255"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\nchar id=65 xoffset=200\n"),
              "fonts/a.fnt:2: xoffset is -128 to 127"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=9\n"), "fonts/a.fnt:1: pages is 1 to 8"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\nchar id=65\n\nchar id=65\n"),
              "fonts/a.fnt:4: character 65 is defined twice (first at line 2)"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\npage id=0 file=\"a_0.png\n"),
              "fonts/a.fnt:2: a quoted value isn't closed"));
    CHECK(has(problems_of("info size=8\nchar id=65\n"), "fonts/a.fnt:1: no \"common\" line (lineHeight, base, pages)"));
    CHECK(has(problems_of("common lineHeight=x base=8 pages=1\n"), "fonts/a.fnt:1: lineHeight is 1 to 255"));
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\nchar id=1114112\n"), ":2: id is 0 to 1114111"));
}

NNH_TEST(bmfont_limits) {
    CHECK(has(problems_of("common lineHeight=10 base=8 pages=1\n" + std::string(5000, 'x') + "\n"),
              "fonts/a.fnt:2: a line longer than 4096 bytes"));
    CHECK(has(problems_of(std::string(kMaxBmFontBytes + 1, ' ')), "fonts/a.fnt:1: larger than 4 MiB"));
}
