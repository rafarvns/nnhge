// Character sets of font descriptions (spec 029 FR-3): every named set's size and first/last code point, ranges, the
// characters of text files.

#include <set>
#include <string>

#include "charsets.hpp"
#include "test.hpp"

using namespace nnh::assetc;

namespace {

struct Set {
    std::set<char32_t> cps;
    bool ok = false;
};

Set named(const char* name) {
    Set s;
    s.ok = add_charset(name, s.cps).empty();
    return s;
}

bool spans(const Set& s, std::size_t size, char32_t first, char32_t last) {
    return s.ok && s.cps.size() == size && *s.cps.begin() == first && *s.cps.rbegin() == last;
}

} // namespace

NNH_TEST(charsets_named_sets_have_their_sizes_and_edges) {
    CHECK(spans(named("ascii"), 95, 0x20, 0x7E));
    CHECK(spans(named("latin1"), 95 + 96, 0x20, 0xFF));
    CHECK(spans(named("latin-ext-a"), 128, 0x100, 0x17F));
    CHECK(spans(named("latin-ext-b"), 208, 0x180, 0x24F));
    CHECK(spans(named("latin-ext-additional"), 256, 0x1E00, 0x1EFF));
    CHECK(named("greek").cps.count(0x3B1) == 1 && named("greek").cps.count(0x3A2) == 0);
    CHECK(spans(named("cyrillic"), 256, 0x400, 0x4FF));
    CHECK(named("armenian").ok && named("georgian").ok && named("arrows").ok && named("box-drawing").ok);
    CHECK(spans(named("general-punctuation"), 112, 0x2000, 0x206F));
    CHECK(spans(named("cjk-punctuation"), 64, 0x3000, 0x303F));
    CHECK(spans(named("hiragana"), 86 + 7, 0x3041, 0x309F));
    CHECK(spans(named("katakana"), 96, 0x30A0, 0x30FF));
    CHECK(spans(named("kana"), 86 + 103, 0x3041, 0x30FF));
    CHECK(spans(named("hangul"), 11172, 0xAC00, 0xD7A3));
    CHECK(named("kyoiku").cps.size() == 1006 && named("kyoiku").cps.count(0x65E5) == 1); // 日
    CHECK(named("jis-level1").cps.size() == 2965 && named("jis-level1").cps.count(0x4E9C) == 1); // 亜, the first
    CHECK(named("fullwidth").ok && named("currency").ok && named("language-names").ok);
}

NNH_TEST(charsets_ranges_and_single_code_points) {
    std::set<char32_t> s;
    CHECK(add_charset("U+20AC", s).empty() && s.count(0x20AC) == 1);
    CHECK(add_charset("U+0100..U+0102", s).empty() && s.size() == 4);
    CHECK(!add_charset("U+0102..U+0100", s).empty());
    CHECK(!add_charset("U+110000", s).empty());
    CHECK(!add_charset("U+D800", s).empty());
    CHECK(add_charset("lating", s) == "unknown character set \"lating\" (did you mean \"latin1\"?)");
}

NNH_TEST(charsets_text_adds_every_character_but_controls) {
    std::set<char32_t> s;
    add_text_characters("Ol\xC3\xA1\n\xE6\x97\xA5\xE6\x9C\xAC\t", s);
    CHECK(s.size() == 5 && s.count(0xE1) == 1 && s.count(0x65E5) == 1 && s.count(U'\n') == 0);
}
