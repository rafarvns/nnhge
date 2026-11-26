// Character sets of font descriptions (charsets.hpp).

#include "charsets.hpp"

#include <string>
#include <vector>

#include "charsets_cjk.hpp"
#include "suggest.hpp"
#include "text/utf8.hpp"

namespace nnh::assetc {
namespace {

struct Range {
    char32_t first, last;
};
struct Named {
    const char* name;
    Range ranges[4];
};

const Named kNamed[] = {
    {"ascii", {{0x20, 0x7E}}},
    {"latin1", {{0x20, 0x7E}, {0xA0, 0xFF}}},
    {"latin-ext-a", {{0x100, 0x17F}}},
    {"latin-ext-b", {{0x180, 0x24F}}},
    {"latin-ext-additional", {{0x1E00, 0x1EFF}}},
    {"greek", {{0x384, 0x38A}, {0x38C, 0x38C}, {0x38E, 0x3A1}, {0x3A3, 0x3CE}}},
    {"cyrillic", {{0x400, 0x4FF}}},
    {"armenian", {{0x531, 0x556}, {0x559, 0x58A}}},
    {"georgian", {{0x10A0, 0x10C5}, {0x10D0, 0x10FF}}},
    {"general-punctuation", {{0x2000, 0x206F}}},
    {"currency", {{0x20A0, 0x20C0}}},
    {"arrows", {{0x2190, 0x21FF}}},
    {"box-drawing", {{0x2500, 0x257F}}},
    {"cjk-punctuation", {{0x3000, 0x303F}}},
    {"hiragana", {{0x3041, 0x3096}, {0x3099, 0x309F}}},
    {"katakana", {{0x30A0, 0x30FF}}},
    {"kana", {{0x3041, 0x3096}, {0x3099, 0x30FF}}},
    {"fullwidth", {{0xFF01, 0xFF60}, {0xFFE0, 0xFFE6}}},
    {"hangul", {{0xAC00, 0xD7A3}}},
};

const std::vector<std::string> kNames = {"ascii", "latin1", "latin-ext-a", "latin-ext-b", "latin-ext-additional", "greek",
                                         "cyrillic", "armenian", "georgian", "general-punctuation", "currency", "arrows",
                                         "box-drawing", "cjk-punctuation", "hiragana", "katakana", "kana", "fullwidth",
                                         "hangul", "jis-level1", "kyoiku", "language-names"};

bool hex_code(std::string_view s, char32_t& out) {
    if (s.size() < 6 || s.size() > 8 || s.substr(0, 2) != "U+") return false;
    char32_t v = 0;
    for (const char c : s.substr(2)) {
        const int d = c >= '0' && c <= '9' ? c - '0' : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1);
        if (d < 0) return false;
        v = v * 16 + static_cast<char32_t>(d);
    }
    out = v;
    return true;
}

bool scalar(char32_t c) { return c <= 0x10FFFF && (c < 0xD800 || c > 0xDFFF); }

std::string add_range(std::string_view entry, std::set<char32_t>& out) {
    const std::size_t dots = entry.find("..");
    const bool range = dots != std::string_view::npos;
    char32_t a = 0, b = 0;
    if (!hex_code(entry.substr(0, range ? dots : entry.size()), a) || (range && !hex_code(entry.substr(dots + 2), b))) {
        return "bad code point \"" + std::string(entry) + "\" (write U+20AC or U+0100..U+017F)";
    }
    if (!range) b = a;
    if (!scalar(a) || !scalar(b) || b < a) return "bad code point range \"" + std::string(entry) + "\"";
    for (char32_t c = a; c <= b; ++c)
        if (scalar(c)) out.insert(c);
    return "";
}

} // namespace

std::string add_charset(std::string_view entry, std::set<char32_t>& out) {
    if (entry.substr(0, 2) == "U+") return add_range(entry, out);
    for (const Named& n : kNamed) {
        if (entry != n.name) continue;
        for (const Range& r : n.ranges)
            for (char32_t c = r.first; r.last != 0 && c <= r.last; ++c) out.insert(c);
        return "";
    }
    if (entry == "kyoiku") {
        for (const std::uint16_t c : kKyoiku) out.insert(c);
        return "";
    }
    if (entry == "jis-level1") {
        for (const std::uint16_t c : kJisLevel1) out.insert(c);
        return "";
    }
    if (entry == "language-names") return "";
    const std::string near = closest(entry, kNames);
    return "unknown character set \"" + std::string(entry) + "\"" + (near.empty() ? "" : " (did you mean \"" + near + "\"?)");
}

void add_text_characters(std::string_view text, std::set<char32_t>& out) {
    const std::string copy(text); // NUL-terminated for the decoder
    for (const char* p = copy.c_str(); *p != 0;) {
        const char32_t c = nnh::detail::next_codepoint(p);
        if (c >= 0x20 && !(c >= 0x7F && c <= 0x9F)) out.insert(c);
    }
}

} // namespace nnh::assetc
