// Wrapping (spec 028 T1.4, FR-5, FR-23): breaks at spaces (dropped) and after hyphens, '\n', long words by characters,
// max lines with "...", Latin-1 two-byte characters as one cell, invalid UTF-8 as one replacement cell.

#include <cstring>

#include "test.hpp"
#include "ui/ui_wrap.hpp"

using nnh::detail::longest_word_cells;
using nnh::detail::WrapLine;
using nnh::detail::Wrapper;

namespace {

/// The lines of `text` at `width` cells, joined with '|' ("..." appended where an ellipsis is drawn).
const char* wrapped(const char* text, int width, int max_lines = 0) {
    static char out[512];
    int n = 0;
    Wrapper w(text, static_cast<int>(std::strlen(text)), width, max_lines);
    WrapLine line;
    bool first = true;
    while (w.next(line)) {
        if (!first) out[n++] = '|';
        first = false;
        std::memcpy(out + n, text + line.start, static_cast<std::size_t>(line.bytes));
        n += line.bytes;
        if (line.ellipsis) {
            std::memcpy(out + n, "...", 3);
            n += 3;
        }
    }
    out[n] = 0;
    return out;
}

int cells_of(const char* text, int width, int max_lines = 0) {
    Wrapper w(text, static_cast<int>(std::strlen(text)), width, max_lines);
    WrapLine line;
    int cells = 0;
    while (w.next(line)) cells += line.cells;
    return cells;
}

bool same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

} // namespace

NNH_TEST(ui_wrap_without_a_width_keeps_lines_whole) {
    CHECK(same(wrapped("Hello world", 0), "Hello world"));
    CHECK(same(wrapped("one\ntwo", 0), "one|two"));
}

NNH_TEST(ui_wrap_breaks_at_spaces_and_drops_them) {
    CHECK(same(wrapped("the quick brown fox", 10), "the quick|brown fox"));
    CHECK(same(wrapped("aa bb  cc", 5), "aa bb|cc"));
}

NNH_TEST(ui_wrap_breaks_after_a_hyphen_and_keeps_it) {
    CHECK(same(wrapped("well-known word", 6), "well-|known|word"));
}

NNH_TEST(ui_wrap_honours_newlines) {
    CHECK(same(wrapped("ab\ncd ef", 5), "ab|cd ef"));
    CHECK(same(wrapped("ab\n\ncd", 5), "ab||cd"));
}

NNH_TEST(ui_wrap_breaks_a_long_word_by_characters) {
    CHECK(same(wrapped("abcdefghij", 4), "abcd|efgh|ij"));
    CHECK(same(wrapped("ab abcdefgh", 4), "ab|abcd|efgh"));
}

NNH_TEST(ui_wrap_exact_fit_stays_on_one_line) {
    CHECK(same(wrapped("abc de", 6), "abc de"));
    CHECK(same(wrapped("abcd", 4), "abcd"));
}

NNH_TEST(ui_wrap_max_lines_ends_with_an_ellipsis_three_cells) {
    // "brown fox" cut to 6 cells ("brown "), its trailing space dropped, then "..."
    CHECK(same(wrapped("the quick brown fox jumps", 9, 2), "the quick|brown..."));
    CHECK(same(wrapped("abcdefghijkl", 5, 2), "abcde|fg..."));
}

NNH_TEST(ui_wrap_exact_fit_in_max_lines_has_no_ellipsis) {
    CHECK(same(wrapped("the quick brown fox", 10, 2), "the quick|brown fox"));
    CHECK(same(wrapped("the quick brown fox  ", 10, 2), "the quick|brown fox")); // spaces at the break dropped
}

NNH_TEST(ui_wrap_latin1_two_byte_characters_are_one_cell) {
    CHECK(same(wrapped("caf\xC3\xA9 ol\xC3\xA9", 4), "caf\xC3\xA9|ol\xC3\xA9"));
    CHECK(cells_of("caf\xC3\xA9 ol\xC3\xA9", 4) == 7);
}

NNH_TEST(ui_wrap_invalid_utf8_is_one_replacement_cell) {
    CHECK(cells_of("a\xFF" "b", 0) == 3);
    CHECK(same(wrapped("ab\xFF" "cd", 3), "ab\xFF|cd"));
}

NNH_TEST(ui_wrap_counts_cells_without_newlines_or_dropped_spaces) {
    CHECK(cells_of("the quick brown fox", 10) == 18); // one space dropped at the break
    CHECK(cells_of("ab\ncd", 0) == 4);
}

NNH_TEST(ui_wrap_every_width_from_1_to_50_keeps_every_line_within_it) {
    const char* text = "Welcome, traveller! The north-east gate opens at dawn; bring 12 coins and a lantern.";
    bool ok = true;
    for (int width = 1; width <= 50; ++width) {
        Wrapper w(text, static_cast<int>(std::strlen(text)), width, 0);
        WrapLine line;
        int total = 0;
        while (w.next(line)) {
            ok = ok && line.cells <= width && line.cells > 0;
            total += line.cells;
        }
        ok = ok && total <= static_cast<int>(std::strlen(text));
    }
    CHECK(ok);
}

NNH_TEST(ui_wrap_longest_word) {
    CHECK(longest_word_cells("a bb ccc", 8) == 3);
    CHECK(longest_word_cells("well-known", 10) == 5); // "well-" and "known"
    CHECK(longest_word_cells("", 0) == 0);
}

NNH_TEST(ui_wrap_empty_text_has_no_line) {
    Wrapper w("", 0, 10, 0);
    WrapLine line;
    CHECK(!w.next(line));
}
