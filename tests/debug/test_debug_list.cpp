// Debug text recorded during the frame and drawn on top at its end (debug/debug_list.hpp, spec 004 FR-6).

#include <cstring>

#include "debug/debug_list.hpp"
#include "test.hpp"

using nnh::Screen;
using nnh::detail::DebugList;
using nnh::detail::DebugText;

namespace {

DebugText g_entries[3];
char g_text[16];

DebugList make() {
    DebugList list;
    list.reset(g_entries, 3, g_text, sizeof g_text);
    return list;
}

} // namespace

NNH_TEST(debug_list_keeps_texts_in_order_with_their_place_and_color) {
    DebugList list = make();
    CHECK(list.add(Screen::Top, 1, 2, "abc", nnh::colors::red));
    CHECK(list.add(Screen::Bottom, 3, 4, "de", nnh::colors::green));
    CHECK(list.count() == 2);
    CHECK(list.at(0).screen == Screen::Top && list.at(0).x == 1 && list.at(0).y == 2);
    CHECK(std::strcmp(list.text(0), "abc") == 0 && std::strcmp(list.text(1), "de") == 0);
    CHECK(list.at(1).color.g == nnh::colors::green.g);
}

NNH_TEST(debug_list_drops_texts_when_entries_run_out) {
    DebugList list = make();
    for (int i = 0; i < 3; ++i) CHECK(list.add(Screen::Top, 0, 0, "x", nnh::colors::white));
    CHECK(!list.add(Screen::Top, 0, 0, "y", nnh::colors::white));
    CHECK(list.count() == 3 && list.dropped() == 1);
}

NNH_TEST(debug_list_drops_texts_that_do_not_fit_the_text_buffer) {
    DebugList list = make();
    CHECK(list.add(Screen::Top, 0, 0, "0123456789", nnh::colors::white)); // 11 bytes with '\0'
    CHECK(!list.add(Screen::Top, 0, 0, "abcdef", nnh::colors::white));   // 7 more: 18 > 16
    CHECK(list.add(Screen::Top, 0, 0, "abcd", nnh::colors::white));      // 5 more: 16 fits
    CHECK(list.count() == 2 && list.dropped() == 1);
    CHECK(std::strcmp(list.text(1), "abcd") == 0);
}

NNH_TEST(debug_list_clear_starts_a_new_frame) {
    DebugList list = make();
    list.add(Screen::Top, 0, 0, "0123456789", nnh::colors::white);
    list.clear();
    CHECK(list.count() == 0 && list.dropped() == 0);
    CHECK(list.add(Screen::Top, 0, 0, "0123456789", nnh::colors::white));
}

NNH_TEST(debug_list_also_keeps_rectangles_in_call_order) {
    DebugList list = make();
    CHECK(list.add(Screen::Bottom, 1, 2, "a", nnh::colors::white));
    CHECK(list.add_rect(Screen::Bottom, 5, 6, 7, 8, nnh::colors::red));
    CHECK(list.count() == 2);
    CHECK(list.at(0).kind == DebugText::Kind::Text);
    const DebugText& r = list.at(1);
    CHECK(r.kind == DebugText::Kind::Rect && r.x == 5 && r.y == 6 && r.w == 7 && r.h == 8);
    CHECK(r.color.r == 255 && r.color.g == 0);
}

NNH_TEST(debug_list_rectangles_use_entries_but_no_text) {
    DebugList list = make();
    CHECK(list.add_rect(Screen::Top, 0, 0, 1, 1, nnh::colors::white));
    CHECK(list.add_rect(Screen::Top, 0, 0, 1, 1, nnh::colors::white));
    CHECK(list.add(Screen::Top, 0, 0, "012345678901234", nnh::colors::white)); // 16 bytes: all the text
    CHECK(!list.add_rect(Screen::Top, 0, 0, 1, 1, nnh::colors::white));        // entries full
    CHECK(list.dropped() == 1);
}
