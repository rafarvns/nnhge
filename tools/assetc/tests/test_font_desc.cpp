// Font descriptions (spec 029 FR-1, FR-3, FR-4; plan "Font descriptions"): every key, the defaults, refusals with their
// line.

#include <string>

#include "font_desc.hpp"
#include "test.hpp"

using namespace nnh::assetc;

namespace {

std::string problems_of(const char* text) {
    FontDesc d;
    Diagnostics p;
    read_font_desc(text, d, p);
    return p.text("fonts/a.font");
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

} // namespace

NNH_TEST(font_desc_reads_every_key) {
    FontDesc d;
    Diagnostics p;
    CHECK(read_font_desc("source = \"NotoSans.otf\"\nsize = 16\nchars = [\"ascii\", \"U+20AC\", \"U+0100..U+017F\"]\n"
                         "chars_from = [\"src/a.chars\"]\nstrings = [\"ja\", \"pt-BR\"]\nantialias = false\nthreshold = 100\n"
                         "padding = 2\noutline = 1\nfallback = [\"fonts/cjk\"]\nreplacement = \"none\"\n"
                         "licence = \"licences/OFL.txt\"\npage = 512\nface = 1\n",
                         d, p));
    CHECK(p.empty());
    CHECK(d.source == "NotoSans.otf" && d.size == 16 && d.chars.size() == 3 && d.chars[2] == "U+0100..U+017F");
    CHECK(d.chars_from.size() == 1 && d.strings.size() == 2 && d.strings[1] == "pt-BR");
    CHECK(!d.antialias && d.threshold == 100 && d.padding == 2 && d.outline == 1 && d.fallback.size() == 1);
    CHECK(!d.replacement && d.licence == "licences/OFL.txt" && d.page == 512 && d.face == 1);
}

NNH_TEST(font_desc_defaults) {
    FontDesc d;
    Diagnostics p;
    CHECK(read_font_desc("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\n", d, p));
    CHECK(d.chars.size() == 1 && d.chars[0] == "ascii" && d.antialias && d.threshold == 128 && d.padding == 1);
    CHECK(d.outline == 0 && d.replacement && d.page == 0 && d.face == 0 && d.fallback.empty());
}

NNH_TEST(font_desc_refuses_missing_and_out_of_range_values) {
    CHECK(has(problems_of("size = 12\nlicence = \"own\"\n"), "fonts/a.font:1: \"source\" is required"));
    CHECK(has(problems_of("source = \"a.ttf\"\nlicence = \"own\"\n"), "fonts/a.font:1: \"size\" is required"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 5\nlicence = \"own\"\n"), "fonts/a.font:2: size is 6 to 128"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\n"), "\"licence\" is required (a file in the assets or \"own\")"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\npadding = 5\n"), ":4: padding is 1 to 4"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\noutline = 4\n"), ":4: outline is 0 to 3"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\npage = 100\n"),
              ":4: page is 0 or a power of two from 64 to 1024"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\nfallback = [\"a\", \"b\", \"c\", \"d\"]\n"),
              ":4: at most 3 fallback fonts"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\nreplacement = \"x\"\n"),
              ":4: replacement is \"auto\" or \"none\""));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\nsizes = 2\n"),
              ":4: unknown key \"sizes\" (did you mean \"size\"?)"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = 12\nlicence = \"own\"\nsize = 13\n"), ":4:"));
    CHECK(has(problems_of("source = \"a.ttf\"\nsize = \"12\"\nlicence = \"own\"\n"), ":2: size is a whole number"));
}
