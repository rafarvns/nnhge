// Text fitting and list rows of NNH Link's views (apps/nnh-link/src/layout.cpp).

#include "layout.hpp"
#include "test.hpp"

using namespace link_app::layout;

NNH_TEST(layout_fit_keeps_short_text_and_cuts_long_text) {
    CHECK(fit("hello", 5) == "hello");
    CHECK(fit("hello!", 5) == "he...");
    CHECK(fit("", 5).empty());
    CHECK(fit("abcdef", 2) == "...");
}

NNH_TEST(layout_fit_counts_characters_not_bytes) {
    CHECK(fit("ação", 4) == "ação");                 // 4 characters, 6 bytes
    CHECK(fit("ççççç", 4) == "ç...");                // never half a ç
}

NNH_TEST(layout_wrap_splits_long_lines_into_rows) {
    const auto rows = wrap({"abcdefg", "", "xy"}, 3);
    CHECK(rows.size() == 5);
    CHECK(rows[0] == "abc");
    CHECK(rows[1] == "def");
    CHECK(rows[2] == "g");
    CHECK(rows[3].empty());
    CHECK(rows[4] == "xy");
}

NNH_TEST(layout_wrap_keeps_utf8_characters_whole_and_turns_tabs_into_spaces) {
    const auto rows = wrap({"ééé\t"}, 2);
    CHECK(rows.size() == 2);
    CHECK(rows[0] == "éé");
    CHECK(rows[1] == "é ");
}

NNH_TEST(layout_rows_scroll_to_keep_the_selection_visible) {
    CHECK(first_row(0) == 0);
    CHECK(first_row(kVisibleRows - 1) == 0);
    CHECK(first_row(kVisibleRows) == 1);
}

NNH_TEST(layout_row_at_finds_the_touched_row) {
    CHECK(row_at(kRowTop - 1, 0, 5) == -1);
    CHECK(row_at(kRowTop, 0, 5) == 0);
    CHECK(row_at(kRowTop + kRowHeight, 0, 5) == 1);
    CHECK(row_at(kRowTop + 5 * kRowHeight, 0, 5) == -1); // below the last row
    CHECK(row_at(kRowTop, kVisibleRows + 2, 20) == 3);    // scrolled by 3
    CHECK(row_at(kRowTop + kVisibleRows * kRowHeight, 0, 20) == -1); // past the visible rows
}

NNH_TEST(layout_button_hit) {
    CHECK(in_button(kButtonX, kButtonY));
    CHECK(in_button(kButtonX + kButtonW - 1, kButtonY + kButtonH - 1));
    CHECK(!in_button(kButtonX - 1, kButtonY));
    CHECK(!in_button(kButtonX, kButtonY + kButtonH));
}
