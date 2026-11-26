// Fuzz bodies of the font and string converters (spec 029 T4.10, T5.8, constitution X): see fuzz_assetc.hpp.

#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "bmfont_text.hpp"
#include "font_desc.hpp"
#include "fuzz_assetc.hpp"
#include "loc/loc_file.hpp"
#include "png_pages.hpp"
#include "strings_check.hpp"
#include "strings_read.hpp"
#include "ttf_check.hpp"
#include "ttf_raster.hpp"

namespace nnh::assetc::fuzz {
namespace {

const char* kBase = "[language]\nname = \"English\"\n[menu]\nnew_game = \"New game\"\nquit = \"Quit\"\n"
                    "[items]\ncount.one = \"{count} item\"\ncount.other = \"{count} items\"\n"
                    "[hud]\nscore = \"Score: {score} {{x}}\"\nstory = \"\"\"\nOnce \\\n  upon\"\"\"\n";

} // namespace

int bmfont_target(const std::uint8_t* data, std::size_t size) {
    BmFont f;
    Diagnostics p;
    if (read_bmfont(std::string_view(reinterpret_cast<const char*>(data), size), f, p)) {
        for (const BmChar& c : f.chars)
            if (c.w < 0 || c.w > 255 || c.h < 0 || c.h > 255 || c.page < 0 || c.page > 7) std::abort();
    } else if (p.empty()) {
        std::abort(); // a refusal names a line and a reason
    }
    return 0;
}

int ttf_target(const std::uint8_t* data, std::size_t size) {
    if (size > kMaxFontFileBytes) return 0;
    for (int face = 0; face < 2; ++face) {
        if (!check_ttf(data, size, face).empty()) continue;
        // accepted: stb_truetype may read it — rasterise a few code points at two sizes (ASan watches)
        const std::vector<unsigned char> file(data, data + size);
        for (const int px : {9, 24}) {
            RasterOptions o;
            o.face = face;
            o.size = px;
            BakedFont out;
            int skipped = 0;
            char32_t first = 0;
            raster_font(file, o, {U' ', U'A', U'V', U'g', U'?', 0x3042, 0x65E5, 0xFFFD}, out, skipped, first);
        }
    }
    return 0;
}

int png_target(const std::uint8_t* data, std::size_t size) {
    int w = 0, h = 0;
    std::vector<std::uint8_t> rgba;
    const std::string why = decode_png(std::string(reinterpret_cast<const char*>(data), size), w, h, rgba);
    if (why.empty() && rgba.size() != static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u) std::abort();
    return 0;
}

int font_desc_target(const std::uint8_t* data, std::size_t size) {
    FontDesc d;
    Diagnostics p;
    if (!read_font_desc(std::string_view(reinterpret_cast<const char*>(data), size), d, p) && p.empty()) std::abort();
    return 0;
}

int strings_target(const std::uint8_t* data, std::size_t size) {
    StringsLanguage base, lang;
    base.code = "en";
    lang.code = "de";
    std::vector<std::string> problems;
    read_strings(kBase, "strings/en.strings", base, problems);
    problems.clear();
    if (!read_strings(std::string_view(reinterpret_cast<const char*>(data), size), "strings/de.strings", lang, problems)) {
        if (problems.empty()) std::abort();
        return 0;
    }
    CheckResult r;
    check_language(lang, base, r);
    if (!r.errors.empty()) return 0;
    for (const bool release : {false, true}) {
        const std::vector<std::uint8_t> t = write_table(lang, release);
        if (detail::loc::validate_table(t.data(), t.size()) != nullptr) std::abort(); // the writer's output is valid
    }
    return 0;
}

const char* strings_seed() {
    return "[language]\nname = \"Deutsch\"\ngrouping = \".\"\n[menu]\nnew_game = \"Neues Spiel\"\nquit = \"Ende\"\n"
           "[items]\ncount.one = \"{count} Ding\"\ncount.other = \"{count} Dinge\"\n"
           "[hud]\nscore = \"Punkte: {score} {{x}}\"\nstory = \"\"\"\nEs \\\n  war\"\"\"\n";
}

const char* bmfont_seed() {
    return "info face=\"P\" size=8\ncommon lineHeight=10 base=8 scaleW=64 scaleH=64 pages=1 packed=0\n"
           "page id=0 file=\"a_0.png\"\nchars count=2\n"
           "char id=65 x=0 y=0 width=7 height=8 xoffset=0 yoffset=1 xadvance=8 page=0 chnl=15\n"
           "char id=66 x=9 y=0 width=7 height=8 xoffset=0 yoffset=1 xadvance=8 page=0 chnl=15\n"
           "kernings count=1\nkerning first=65 second=66 amount=-1\n";
}

const char* font_desc_seed() {
    return "source = \"a.ttf\"\nsize = 16\nchars = [\"ascii\", \"U+20AC\", \"U+0100..U+017F\", \"kana\"]\n"
           "strings = [\"ja\"]\nantialias = false\nthreshold = 100\npadding = 2\noutline = 1\n"
           "fallback = [\"fonts/b\"]\nreplacement = \"none\"\nlicence = \"own\"\npage = 512\n";
}

} // namespace nnh::assetc::fuzz
