// The shared wrapper (spec 029 FR-10; spec 028 FR-5, FR-23; plan "Wrapping"): one implementation for draw_text and the
// UI's labels, in pixels through a face. Spec 028's cases (moved from tests/ui/test_wrap.cpp) keep their expected lines,
// with widths as cells × 8 through the built-in face; then the new rules: U+2010, U+3000, ID breaks, Hangul at spaces
// only, kinsoku with the back-off rule, the ellipsis "…" of a game font.

#include <type_traits>
#include <cstring>
#include <vector>

#include "../font/font_fixture.hpp"
#include "font/font_block.hpp"
#include "test.hpp"
#include "text/text_wrap.hpp"
#include "text/utf8.hpp"

using namespace nnh::detail;
using font_test::FontBuilder;

namespace {

const TextFace kBuiltin{};

/// The lines of `text` at `width` pixels, joined with '|' (the ellipsis bytes appended where one is drawn).
const char* wrapped(const TextFace& face, const char* text, int width, int max_lines = 0) {
    static char out[1024];
    int n = 0;
    TextWrapper w(face, text, static_cast<int>(std::strlen(text)), width, max_lines);
    WrapLine line;
    bool first = true;
    while (w.next(line)) {
        if (!first) out[n++] = '|';
        first = false;
        std::memcpy(out + n, text + line.start, static_cast<std::size_t>(line.bytes));
        n += line.bytes;
        if (line.ellipsis) {
            const char* e = ellipsis_text(face);
            std::memcpy(out + n, e, std::strlen(e));
            n += static_cast<int>(std::strlen(e));
        }
    }
    out[n] = 0;
    return out;
}

/// Spec 028's tests: `width` in cells of the built-in font.
const char* cells(const char* text, int width, int max_lines = 0) { return wrapped(kBuiltin, text, width * 8, max_lines); }

int chars_of(const char* text, int width_cells, int max_lines = 0) {
    TextWrapper w(kBuiltin, text, static_cast<int>(std::strlen(text)), width_cells * 8, max_lines);
    WrapLine line;
    int chars = 0;
    while (w.next(line)) chars += line.chars;
    return chars;
}

bool same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

struct Block {
    std::vector<std::uint64_t> mem;
    const FontBlock* font = nullptr;
    TextFace face() const {
        TextFace f;
        f.fonts[0] = font;
        f.count = 1;
        return f;
    }
};

Block make(const FontBuilder& b) {
    Block out;
    const auto f = b.build();
    const font::Header h = font::read_header(f.data());
    out.mem.assign(font_block_bytes(h) / 8 + 1, 0);
    out.font = font_block_init(out.mem.data(), font_block_bytes(h), f.data(), h.off_page_data);
    return out;
}

/// A CJK font: every glyph 10 pixels wide, advancing 10.
FontBuilder cjk() {
    FontBuilder b;
    b.page_side = 128;
    b.page_height = 128;
    const char* chars = "あいうえおかきくけこ日本語漢字。、「」ーぁァ（）！？‐가나다 a";
    int i = 0;
    for (const char* p = chars;;) {
        const char32_t c = next_codepoint(p);
        if (c == 0) break;
        if (c == U' ') b.glyph(c, 0, 0, 0, 0, 0, 0, 10);
        else if (c == U'a') b.glyph(c, (i % 12) * 10, (i / 12) * 10, 6, 8, 0, 0, 6);
        else b.glyph(c, (i % 12) * 10, (i / 12) * 10, 10, 10, 0, 0, 10);
        ++i;
    }
    b.glyph(0x3000, 0, 0, 0, 0, 0, 0, 10);
    return b;
}

} // namespace

// ---- spec 028's cases (tests/ui/test_wrap.cpp before spec 029), unchanged lines ----

NNH_TEST(text_wrap_without_a_width_keeps_lines_whole) {
    CHECK(same(cells("Hello world", 0), "Hello world"));
    CHECK(same(cells("one\ntwo", 0), "one|two"));
}

NNH_TEST(text_wrap_breaks_at_spaces_and_drops_them) {
    CHECK(same(cells("the quick brown fox", 10), "the quick|brown fox"));
    CHECK(same(cells("aa bb  cc", 5), "aa bb|cc"));
}

NNH_TEST(text_wrap_breaks_after_a_hyphen_and_keeps_it) { CHECK(same(cells("well-known word", 6), "well-|known|word")); }

NNH_TEST(text_wrap_honours_newlines) {
    CHECK(same(cells("ab\ncd ef", 5), "ab|cd ef"));
    CHECK(same(cells("ab\n\ncd", 5), "ab||cd"));
}

NNH_TEST(text_wrap_breaks_a_long_word_by_characters) {
    CHECK(same(cells("abcdefghij", 4), "abcd|efgh|ij"));
    CHECK(same(cells("ab abcdefgh", 4), "ab|abcd|efgh"));
}

NNH_TEST(text_wrap_exact_fit_stays_on_one_line) {
    CHECK(same(cells("abc de", 6), "abc de"));
    CHECK(same(cells("abcd", 4), "abcd"));
}

NNH_TEST(text_wrap_max_lines_ends_with_an_ellipsis_three_cells) {
    CHECK(same(cells("the quick brown fox jumps", 9, 2), "the quick|brown..."));
    CHECK(same(cells("abcdefghijkl", 5, 2), "abcde|fg..."));
}

NNH_TEST(text_wrap_exact_fit_in_max_lines_has_no_ellipsis) {
    CHECK(same(cells("the quick brown fox", 10, 2), "the quick|brown fox"));
    CHECK(same(cells("the quick brown fox  ", 10, 2), "the quick|brown fox"));
}

NNH_TEST(text_wrap_latin1_two_byte_characters_are_one_cell) {
    CHECK(same(cells("caf\xC3\xA9 ol\xC3\xA9", 4), "caf\xC3\xA9|ol\xC3\xA9"));
    CHECK(chars_of("caf\xC3\xA9 ol\xC3\xA9", 4) == 7);
}

NNH_TEST(text_wrap_invalid_utf8_is_one_replacement_cell) {
    CHECK(chars_of("a\xFF" "b", 0) == 3);
    CHECK(same(cells("ab\xFF" "cd", 3), "ab\xFF|cd"));
}

NNH_TEST(text_wrap_counts_characters_without_newlines_or_dropped_spaces) {
    CHECK(chars_of("the quick brown fox", 10) == 18);
    CHECK(chars_of("ab\ncd", 0) == 4);
}

NNH_TEST(text_wrap_every_width_from_1_to_50_cells_keeps_every_line_within_it) {
    const char* text = "Welcome, traveller! The north-east gate opens at dawn; bring 12 coins and a lantern.";
    bool ok = true;
    for (int width = 1; width <= 50; ++width) {
        TextWrapper w(kBuiltin, text, static_cast<int>(std::strlen(text)), width * 8, 0);
        WrapLine line;
        int total = 0;
        while (w.next(line)) {
            ok = ok && line.chars <= width && line.chars > 0 && line.width == line.chars * 8;
            total += line.chars;
        }
        ok = ok && total <= static_cast<int>(std::strlen(text));
    }
    CHECK(ok);
}

NNH_TEST(text_wrap_longest_word) {
    CHECK(longest_word_width(kBuiltin, "a bb ccc", 8) == 24);
    CHECK(longest_word_width(kBuiltin, "well-known", 10) == 40);
    CHECK(longest_word_width(kBuiltin, "", 0) == 0);
}

NNH_TEST(text_wrap_empty_text_has_no_line) {
    TextWrapper w(kBuiltin, "", 0, 80, 0);
    WrapLine line;
    CHECK(!w.next(line));
}

// ---- spec 029's rules ----

NNH_TEST(text_wrap_widths_are_in_pixels_at_the_faces_scale) {
    TextFace big;
    big.scale = 2;
    CHECK(same(wrapped(big, "the quick brown fox", 160), "the quick|brown fox")); // 10 cells of 16 pixels
    CHECK(same(wrapped(big, "the quick brown fox", 159), "the quick|brown fox")); // 9 cells
    CHECK(same(wrapped(big, "the quick brown fox", 143), "the|quick|brown|fox")); // 8 cells
}

NNH_TEST(text_wrap_after_u2010_and_at_ideographic_spaces) {
    const Block b = make(cjk());
    CHECK(same(wrapped(b.face(), "日本‐語漢", 30), "日本‐|語漢")); // after the hyphen
    CHECK(same(wrapped(b.face(), "日本\xE3\x80\x80語漢字", 30), "日本|語漢字")); // U+3000 dropped at the break
}

NNH_TEST(text_wrap_breaks_between_ideographic_characters) {
    const Block b = make(cjk());
    CHECK(same(wrapped(b.face(), "あいうえおかきくけこ", 30), "あいう|えおか|きくけ|こ"));
    CHECK(same(wrapped(b.face(), "日本語a漢字", 30), "日本|語a漢|字")); // no break next to 'a' (not ID)
}

NNH_TEST(text_wrap_hangul_breaks_at_spaces_only) {
    const Block b = make(cjk());
    CHECK(same(wrapped(b.face(), "가나다 가나", 40), "가나다|가나"));
    CHECK(same(wrapped(b.face(), "가나다가나", 30), "가나다|가나")); // a word longer than the line: by characters
}

NNH_TEST(text_wrap_no_line_starts_with_a_no_start_character) {
    const Block b = make(cjk());
    CHECK(same(wrapped(b.face(), "あいう。えお", 30), "あい|う。え|お"));   // 。 isn't ID: no break before it anyway
    CHECK(same(wrapped(b.face(), "あいうーえお", 30), "あい|うーえ|お"));   // ー is ID and no-start: back one
    // before the 2nd ぁ: back once (before the 1st, no-start too), twice (before い): allowed. Then in "いぁぁぁう" the
    // line start is never a break: no allowed place within two characters, so it breaks where it was (before a ぁ)
    CHECK(same(wrapped(b.face(), "あいぁぁぁう", 30), "あ|いぁぁ|ぁう"));
}

NNH_TEST(text_wrap_no_line_ends_with_a_no_end_character) {
    const Block b = make(cjk());
    CHECK(same(wrapped(b.face(), "あい（うえ", 30), "あい|（うえ"));       // （ is ID and no-end: the break moves before it
}

NNH_TEST(text_wrap_ellipsis_is_the_fonts_u2026_else_three_dots) {
    FontBuilder with = FontBuilder::latin();
    const Block b = make(with);
    // latin(): lowercase advance 6, '…' advance 9; width 40: "abcde" (30) + … (9) = 39
    CHECK(same(wrapped(b.face(), "abcdefg hij", 40, 1), "abcde\xE2\x80\xA6"));
    FontBuilder without;
    without.glyph(U'.', 0, 0, 2, 2, 0, 0, 3).glyph(U'a', 8, 0, 5, 5, 0, 0, 6).glyph(U' ', 0, 0, 0, 0, 0, 0, 4);
    const Block c = make(without);
    CHECK(same(ellipsis_text(c.face()), "..."));
    CHECK(same(wrapped(c.face(), "aaaaaaa aa", 30, 1), "aaa...")); // 3 × 6 + 9 ≤ 30
}

NNH_TEST(text_wrap_kerning_and_ink_count_in_a_lines_width) {
    FontBuilder k;
    k.glyph(U' ', 0, 0, 0, 0, 0, 0, 4).glyph(U'A', 0, 0, 7, 9, 0, 0, 8).glyph(U'V', 8, 0, 7, 9, 0, 0, 8);
    k.kern(U'A', U'V', -2);
    const Block b = make(k);
    CHECK(same(wrapped(b.face(), "AV AV", 14), "AV|AV")); // 8 − 2 + 8 = 14 fits; " AV" doesn't
    CHECK(same(wrapped(b.face(), "AV", 13), "A|V"));
    const TextFace face = b.face(); // the wrapper keeps a reference: the face must outlive it
    TextWrapper w(face, "AV", 2, 0, 0);
    WrapLine line;
    CHECK(w.next(line) && line.width == 14 && line.chars == 2);
}

// A wrapper keeps a reference to its face, so a temporary face can't be passed (it dangled: ASan, Linux, 2026-11-26).
static_assert(!std::is_constructible_v<TextWrapper, TextFace&&, const char*, int, int, int>,
              "TextWrapper must refuse a temporary TextFace");
static_assert(std::is_constructible_v<TextWrapper, const TextFace&, const char*, int, int, int>);
