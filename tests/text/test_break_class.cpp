// Line-break classes (spec 029 FR-10, FR-12, plan "Wrapping"): break spaces, hyphens, ideographic (ID) ranges, Hangul
// (not ID), combining and zero-width characters, and the kinsoku tables (JIS X 4051: no line starts / ends with). The
// expected sets are written here from plan 029's lists, not from the code's table.

#include <initializer_list>

#include "test.hpp"
#include "text/break_class.hpp"
#include "text/utf8.hpp"

using namespace nnh::detail;

namespace {

int kind(char32_t c) { return break_class(c) & kBreakKindMask; }
bool no_start(char32_t c) { return (break_class(c) & kBreakNoStart) != 0; }
bool no_end(char32_t c) { return (break_class(c) & kBreakNoEnd) != 0; }
bool zero_width(char32_t c) { return (break_class(c) & kBreakZeroWidth) != 0; }

/// Every code point of `utf8` passes `test`.
template <typename Fn>
bool all_of(const char* utf8, Fn&& test) {
    for (const char* p = utf8;;) {
        const char32_t c = next_codepoint(p);
        if (c == 0) return true;
        if (!test(c)) return false;
    }
}

} // namespace

NNH_TEST(break_class_latin1_is_spec_028s_space_hyphen_and_other) {
    for (char32_t c = 0; c <= 0xFF; ++c) {
        const int want = c == U' ' ? kBreakSpace : (c == U'-' ? kBreakHyphen : kBreakOther);
        CHECK(break_class(c) == want); // no kinsoku or zero-width bit below U+0100: Latin-1 wraps as before
    }
}

NNH_TEST(break_class_spaces_and_hyphens_beyond_latin1) {
    CHECK(kind(0x3000) == kBreakSpace);                       // ideographic space
    CHECK(kind(0x00A0) == kBreakOther);                       // no-break space never breaks
    CHECK(kind(0x2010) == kBreakHyphen && no_start(0x2010)); // ‐ breaks after, never starts a line
    CHECK(kind(0x2011) == kBreakOther);                       // non-breaking hyphen
}

NNH_TEST(break_class_ideographic_range_edges) {
    const char32_t ranges[][2] = {{0x2E80, 0x2FDF}, {0x3005, 0x3007}, {0x3021, 0x3029}, {0x3038, 0x303B},
                                  {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xF900, 0xFAFF}, {0x20000, 0x3FFFF},
                                  {0x3041, 0x30FF}, {0xFF01, 0xFF60}, {0xFFE0, 0xFFE6}, {0xFF65, 0xFF9F}};
    for (const auto& r : ranges) {
        CHECK(kind(r[0]) == kBreakIdeographic);
        CHECK(kind(r[1]) == kBreakIdeographic);
        CHECK(kind((r[0] + r[1]) / 2) == kBreakIdeographic);
    }
    // just outside (where no other range or table entry starts)
    for (char32_t c : {char32_t{0x2E7F}, char32_t{0x2FE0}, char32_t{0x3004}, char32_t{0x3020}, char32_t{0x302A},
                       char32_t{0x3037}, char32_t{0x303C}, char32_t{0x33FF}, char32_t{0x4DC0}, char32_t{0x4DFF},
                       char32_t{0xA000}, char32_t{0xF8FF}, char32_t{0xFB00}, char32_t{0x1FFFF}, char32_t{0x40000},
                       char32_t{0x3040}, char32_t{0x3100}, char32_t{0xFF00}, char32_t{0xFFDF}, char32_t{0xFFE7}, char32_t{0xFFA0}}) {
        CHECK(kind(c) != kBreakIdeographic);
    }
}

NNH_TEST(break_class_hangul_is_not_ideographic) {
    CHECK(kind(0xAC00) == kBreakOther && kind(0xD7A3) == kBreakOther); // breaks at spaces only
    CHECK(kind(0x1100) == kBreakOther);
}

NNH_TEST(break_class_every_no_start_entry) {
    const char* kNoStart = "、。，．・：；？！゛゜ヽヾゝゞ々〻ー‐」』）〕］｝〉》】〙〗〟’”｠…‥〜"
                           "ぁぃぅぇぉっゃゅょゎゕゖァィゥェォッャュョヮヵヶ！％），．：；？］｝｡｣､･ｰ";
    CHECK(all_of(kNoStart, [](char32_t c) { return no_start(c) && !no_end(c) && c >= 0x2000; }));
    CHECK(!no_start(U'あ') && !no_start(U'ア') && !no_start(U'日') && !no_start(U'「'));
}

NNH_TEST(break_class_every_no_end_entry) {
    const char* kNoEnd = "「『（〔［｛〈《【〘〖〝‘“｟";
    CHECK(all_of(kNoEnd, [](char32_t c) { return no_end(c) && !no_start(c) && c >= 0x2000; }));
    CHECK(!no_end(U'」') && !no_end(U'あ'));
}

NNH_TEST(break_class_kinsoku_entries_keep_their_kind) {
    CHECK(kind(U'ぁ') == kBreakIdeographic && no_start(U'ぁ')); // small kana are ID and no-start
    CHECK(kind(U'ー') == kBreakIdeographic && no_start(U'ー'));
    CHECK(kind(U'（') == kBreakIdeographic && no_end(U'（'));   // fullwidth forms are ID
    CHECK(kind(U'」') == kBreakOther && no_start(U'」'));       // CJK punctuation is not ID
    CHECK(kind(U'…') == kBreakOther && no_start(U'…'));
}

NNH_TEST(break_class_combining_and_zero_width_characters) {
    const char32_t ranges[][2] = {{0x0300, 0x036F}, {0x0483, 0x0489}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF},
                                  {0x200B, 0x200D}, {0x2060, 0x2060}, {0x20D0, 0x20FF}, {0x3099, 0x309A},
                                  {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F}, {0xFEFF, 0xFEFF}, {0xE0100, 0xE01EF}};
    for (const auto& r : ranges) {
        CHECK(zero_width(r[0]) && zero_width(r[1]));
        CHECK(!zero_width(r[0] - 1) && !zero_width(r[1] + 1));
    }
    CHECK(kind(0x3099) == kBreakOther); // a combining mark breaks nothing (inside the kana block)
}

NNH_TEST(break_class_beyond_the_table) {
    CHECK(break_class(0x10FFFF) == kBreakOther);
    CHECK(break_class(0x110000) == kBreakOther);
    CHECK(break_class(0xFFFD) == kBreakOther);
}

NNH_TEST(break_class_inline_helpers_agree_with_the_table_for_every_code_point) {
    // T10.3: is_zero_width() and break_class_fast() answer without the search; every code point checked
    int wrong = 0;
    for (char32_t c = 0; c <= 0x10FFFF; ++c) {
        const std::uint8_t bits = nnh::detail::break_class(c);
        wrong += nnh::detail::is_zero_width(c) != ((bits & nnh::detail::kBreakZeroWidth) != 0) ? 1 : 0;
        wrong += nnh::detail::break_class_fast(c) != bits ? 1 : 0;
    }
    CHECK(wrong == 0);
}
