// Lists (spec 028 T5.1, FR-11, FR-15): whole-row scrolling with a margin, pages, wrap on a fresh press, count changes
// clamping focus and scroll, set_scroll/set_item/list_info, the row fill called only for rows coming into view or
// marked changed, disabled rows skipped or refused, 65 535 items, and 20 vs 10 000 items doing the same work.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiListInfo;
using nnh::UiResultKind;
using nnh::UiRow;
using nnh::UiWidget;
using ui_test::hold;
using ui_test::kConfirm;
using ui_test::kDown;
using ui_test::kPageForward;
using ui_test::kUp;
using ui_test::press;
using ui_test::tap;

namespace {

int g_fills = 0;
int g_disabled_every = 0; // 0: none

void fill(int index, UiRow& row) {
    ++g_fills;
    row.text = "item";
    row.number = index;
    row.enabled = g_disabled_every == 0 || index % g_disabled_every != 0 || index == 0;
}

struct ListUi {
    Ui ui;
    UiWidget list;
    ListUi(int count, int rows = 4, int margin = 1, bool wrap = true, bool inert = false)
        : ui(ui_test::make({.rows = 64})) {
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom), {.disabled_focusable = inert});
        list = ui.list(box, {.rows = rows, .count = count, .fill = &fill, .margin = margin, .wrap = wrap});
        ui_test::settle(ui);
    }
    UiListInfo info() const { return ui.list_info(list); }
};

} // namespace

NNH_TEST(ui_list_moves_and_scrolls_by_whole_rows_with_a_margin) {
    ListUi m(20);
    CHECK(m.ui.focused() == m.list && m.info().focused == 0 && m.info().first_visible == 0);
    tap(m.ui, kDown);
    tap(m.ui, kDown);
    CHECK(m.info().focused == 2 && m.info().first_visible == 0); // rows 0..3 shown, one row of margin below 2
    tap(m.ui, kDown);
    CHECK(m.info().focused == 3 && m.info().first_visible == 1);
    CHECK(m.ui.results().data[0].kind == UiResultKind::FocusChanged && m.ui.results().data[0].index == 3);
    tap(m.ui, kUp);
    tap(m.ui, kUp);
    CHECK(m.info().focused == 1 && m.info().first_visible == 0);
}

NNH_TEST(ui_list_margin_zero_scrolls_at_the_edge) {
    ListUi m(20, 4, 0);
    for (int i = 0; i < 4; ++i) tap(m.ui, kDown);
    CHECK(m.info().focused == 4 && m.info().first_visible == 1);
}

NNH_TEST(ui_list_wraps_on_a_fresh_press_not_while_held) {
    ListUi m(6);
    tap(m.ui, kUp);
    CHECK(m.info().focused == 5 && m.info().first_visible == 2);
    m.ui.update(UiInput{});
    m.ui.update(press(kDown));
    CHECK(m.info().focused == 0);
    for (int f = 0; f < 24 + 6 * 8; ++f) m.ui.update(hold(kDown)); // 5 at most: stops at the bottom
    CHECK(m.info().focused == 5);
}

NNH_TEST(ui_list_pages_by_visible_rows) {
    ListUi m(100);
    tap(m.ui, kPageForward);
    CHECK(m.info().focused == 4 && m.info().first_visible == 4);
    CHECK(ui_test::has(m.ui, UiResultKind::Page));
    tap(m.ui, ui_test::kPageBack);
    CHECK(m.info().focused == 0 && m.info().first_visible == 0);
}

NNH_TEST(ui_list_activation_carries_the_item) {
    ListUi m(10);
    tap(m.ui, kDown);
    tap(m.ui, kConfirm);
    CHECK(m.ui.activated(m.list) && m.ui.results().data[0].index == 1);
    CHECK(m.ui.value(m.list) == 1);
}

NNH_TEST(ui_list_count_changes_clamp_focus_and_scroll) {
    ListUi m(50);
    m.ui.set_item(m.list, 40);
    CHECK(m.info().focused == 40 && m.info().first_visible == 38); // 40 - (4 - 1 - margin 1)
    m.ui.set_item_count(m.list, 10);
    CHECK(m.info().count == 10 && m.info().focused == 9 && m.info().first_visible == 6);
    m.ui.set_item_count(m.list, 0);
    CHECK(m.info().count == 0 && m.info().focused == -1 && !m.ui.focused());
}

NNH_TEST(ui_list_set_scroll_is_clamped) {
    ListUi m(30);
    m.ui.set_scroll(m.list, 12);
    CHECK(m.info().first_visible == 12);
    m.ui.set_scroll(m.list, 100);
    CHECK(m.info().first_visible == 26);
    m.ui.set_scroll(m.list, -3);
    CHECK(m.info().first_visible == 0 && m.info().visible_rows == 4);
}

NNH_TEST(ui_list_fills_only_rows_coming_into_view) {
    ui_test::restart();
    {
        ListUi m(1000, 12);
        g_fills = 0;
        CHECK(nnh::frame());
        CHECK(nnh::frame());
        m.ui.draw();
        CHECK(g_fills == 12);
        for (int i = 0; i < 10; ++i) tap(m.ui, kDown); // inside the window
        CHECK(nnh::frame());
        m.ui.draw();
        CHECK(g_fills == 12);
        tap(m.ui, kDown); // the window moves one row
        CHECK(nnh::frame());
        m.ui.draw();
        CHECK(g_fills == 13);
        m.ui.mark_items_changed(m.list, 5, 2);
        CHECK(nnh::frame());
        m.ui.draw();
        CHECK(g_fills == 15);
        m.ui.mark_items_changed(m.list);
        CHECK(nnh::frame());
        m.ui.draw();
        CHECK(g_fills == 27);
    }
    nnh::shutdown();
}

NNH_TEST(ui_list_20_and_10000_items_do_the_same_work) {
    ui_test::restart();
    int fills[2] = {};
    int counts[2] = {20, 10000};
    for (int k = 0; k < 2; ++k) {
        ListUi m(counts[k], 12, 1, false);
        g_fills = 0;
        CHECK(nnh::frame());
        for (int i = 0; i < 16; ++i) {
            tap(m.ui, kDown);
            CHECK(nnh::frame());
            m.ui.draw();
        }
        for (int i = 0; i < 16; ++i) {
            tap(m.ui, kUp);
            CHECK(nnh::frame());
            m.ui.draw();
        }
        fills[k] = g_fills;
    }
    CHECK(fills[0] == fills[1] && fills[0] > 0);
    nnh::shutdown();
}

NNH_TEST(ui_list_disabled_rows_are_skipped) {
    g_disabled_every = 3;
    ListUi m(20);
    tap(m.ui, kDown);
    tap(m.ui, kDown);
    CHECK(m.info().focused == 2);
    tap(m.ui, kDown); // 3 is disabled
    CHECK(m.info().focused == 4);
    g_disabled_every = 0;
}

NNH_TEST(ui_list_disabled_rows_are_refused_when_inert) {
    g_disabled_every = 3;
    ListUi m(20, 4, 1, true, true);
    for (int i = 0; i < 3; ++i) tap(m.ui, kDown);
    CHECK(m.info().focused == 3);
    tap(m.ui, kConfirm);
    CHECK(!m.ui.activated(m.list) && ui_test::has(m.ui, UiResultKind::Refused));
    g_disabled_every = 0;
}

NNH_TEST(ui_list_of_65535_items) {
    ListUi m(65535, 8);
    m.ui.set_item(m.list, 65534);
    CHECK(m.info().focused == 65534 && m.info().first_visible == 65527);
    tap(m.ui, kDown); // wraps on a fresh press
    CHECK(m.info().focused == 0 && m.info().first_visible == 0);
}
