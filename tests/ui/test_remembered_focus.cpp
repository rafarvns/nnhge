// Default and remembered focus (spec 028 T3.4, FR-17): entering a container goes to its last focused child if still a
// target, else its default, else its first target; lists remember item and scroll; a rebuilt menu restores by key.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using ui_test::K;
using ui_test::kDown;
using ui_test::kRight;
using ui_test::kUp;
using ui_test::tap;

namespace {

struct Rows {
    Ui ui = ui_test::make({.rows = 16});
    UiWidget col, r1, r2, x1, x2, x3, y1, y2;
    Rows() {
        col = ui.vbox(ui.root(Screen::Bottom));
        r1 = ui.hbox(col);
        x1 = ui.button(r1, {.text = "x1"});
        x2 = ui.button(r1, {.text = "x2"});
        x3 = ui.button(r1, {.text = "x3"});
        r2 = ui.hbox(col);
        y1 = ui.button(r2, {.text = "y1"});
        y2 = ui.button(r2, {.text = "y2"});
        ui_test::settle(ui);
    }
};

} // namespace

NNH_TEST(ui_remembered_entering_goes_to_the_first_target) {
    Rows m;
    m.ui.focus(m.x3);
    tap(m.ui, kDown); // the column steps to r2: its first target
    CHECK(m.ui.focused() == m.y1);
}

NNH_TEST(ui_remembered_returning_goes_to_the_last_focused) {
    Rows m;
    m.ui.focus(m.x3);
    tap(m.ui, kDown);
    tap(m.ui, kRight);
    CHECK(m.ui.focused() == m.y2);
    tap(m.ui, kUp);
    CHECK(m.ui.focused() == m.x3);
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.y2);
}

NNH_TEST(ui_remembered_default_when_nothing_was_focused) {
    Rows m;
    m.ui.set_default(m.r2, m.y2);
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.y2);
}

NNH_TEST(ui_remembered_child_no_longer_a_target_falls_back_to_the_default) {
    Rows m;
    m.ui.focus(m.y2);
    m.ui.focus(m.x1);
    m.ui.set_enabled(m.y2, false);
    m.ui.set_default(m.r2, m.y1);
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.y1);
}

NNH_TEST(ui_remembered_root_default_is_the_first_focus) {
    Ui ui = ui_test::make();
    ui.button(ui.root(Screen::Bottom), {.text = "a"});
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b"});
    ui.set_default(ui.root(Screen::Bottom), b);
    ui_test::settle(ui);
    CHECK(ui.focused() == b);
}

NNH_TEST(ui_remembered_a_rebuilt_menu_restores_focus_by_key) {
    Ui ui = ui_test::make();
    UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "New", .key = K::A});
    ui.button(box, {.text = "Continue", .key = K::B});
    ui_test::settle(ui);
    tap(ui, kDown);
    const nnh::UiKey kept = ui.focused_key();
    ui.destroy(box);
    box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "New", .key = K::A});
    const UiWidget cont = ui.button(box, {.text = "Continue", .key = K::B});
    ui.focus(kept);
    CHECK(ui.focused() == cont);
}

NNH_TEST(ui_remembered_a_list_keeps_its_item_and_scroll) {
    Ui ui = ui_test::make({.rows = 8});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget list = ui.list(box, {.rows = 4, .count = 50, .fill = [](int, nnh::UiRow&) {}, .wrap = false});
    const UiWidget after = ui.button(box, {.text = "after"});
    ui_test::settle(ui);
    for (int i = 0; i < 10; ++i) tap(ui, kDown);
    const nnh::UiListInfo before = ui.list_info(list);
    CHECK(before.focused == 10);
    ui.focus(after);
    ui.focus(list);
    CHECK(ui.list_info(list).focused == 10 && ui.list_info(list).first_visible == before.first_visible);
}
