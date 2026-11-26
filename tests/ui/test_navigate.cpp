// Navigation (spec 028 T3.3, FR-16): boxes along their axis, wrap on a fresh press only (or on repeat by option), grid
// boxes by columns, explicit neighbours first, sliders/choices/toggles consuming or passing, the geometric rule across
// boxes, blocked moves keeping focus, and no automatic move to the other root.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiDir;
using nnh::UiInput;
using nnh::UiResultKind;
using nnh::UiWidget;
using ui_test::hold;
using ui_test::kDown;
using ui_test::kLeft;
using ui_test::kRight;
using ui_test::kUp;
using ui_test::press;
using ui_test::tap;

namespace {

struct Column {
    Ui ui = ui_test::make({.texts = 4});
    UiWidget box, a, b, c;
    explicit Column(const nnh::UiBoxDesc& d = {}) {
        box = ui.vbox(ui.root(Screen::Bottom), d);
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        c = ui.button(box, {.text = "c"});
        ui_test::settle(ui);
    }
};

/// Holds `dir` for `frames` frames after its press.
void hold_for(Ui& ui, std::uint8_t dir, int frames) {
    ui.update(press(dir));
    for (int i = 1; i < frames; ++i) ui.update(hold(dir));
}

} // namespace

NNH_TEST(ui_nav_vbox_moves_up_and_down) {
    Column m;
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.b && ui_test::has(m.ui, UiResultKind::FocusChanged));
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.c);
    tap(m.ui, kUp);
    CHECK(m.ui.focused() == m.b);
}

NNH_TEST(ui_nav_wraps_on_a_fresh_press) {
    Column m;
    tap(m.ui, kUp);
    CHECK(m.ui.focused() == m.c);
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.a);
}

NNH_TEST(ui_nav_holding_stops_at_the_end) {
    Column m;
    hold_for(m.ui, kDown, 24 + 6 * 5); // press, 24, 30, ...: many repeats
    CHECK(m.ui.focused() == m.c);
    CHECK(ui_test::has(m.ui, UiResultKind::Blocked) || m.ui.results().size == 0);
}

NNH_TEST(ui_nav_wrap_on_repeat_by_option) {
    Column m({.wrap_on_repeat = true});
    hold_for(m.ui, kDown, 25); // press (b), repeat at 24 (c)
    CHECK(m.ui.focused() == m.c);
    for (int i = 0; i < 6; ++i) m.ui.update(hold(kDown)); // repeat at 30: wraps
    CHECK(m.ui.focused() == m.a);
}

NNH_TEST(ui_nav_without_wrap_the_end_is_blocked) {
    Column m({.wrap = false});
    tap(m.ui, kUp);
    CHECK(m.ui.focused() == m.a && ui_test::count(m.ui, UiResultKind::Blocked) == 1);
}

NNH_TEST(ui_nav_hbox_moves_left_and_right) {
    Ui ui = ui_test::make();
    const UiWidget row = ui.hbox(ui.root(Screen::Bottom));
    const UiWidget a = ui.button(row, {.text = "a"});
    const UiWidget b = ui.button(row, {.text = "b"});
    ui_test::settle(ui);
    tap(ui, kRight);
    CHECK(ui.focused() == b);
    tap(ui, kRight);
    CHECK(ui.focused() == a); // wraps
    tap(ui, kDown);
    CHECK(ui.focused() == a && ui_test::has(ui, UiResultKind::Blocked));
}

NNH_TEST(ui_nav_grid_box_moves_by_columns) {
    Ui ui = ui_test::make();
    const UiWidget g = ui.grid_box(ui.root(Screen::Bottom), {.columns = 3});
    UiWidget w[7];
    for (UiWidget& x : w) x = ui.button(g, {.text = "x"});
    ui_test::settle(ui);
    tap(ui, kRight);
    CHECK(ui.focused() == w[1]);
    tap(ui, kDown);
    CHECK(ui.focused() == w[4]);
    tap(ui, kDown);
    CHECK(ui.focused() == w[1]); // column 1 has no third row: wraps to its top
    tap(ui, kLeft);
    tap(ui, kLeft);
    CHECK(ui.focused() == w[2]); // wraps within the row
    tap(ui, kUp);
    CHECK(ui.focused() == w[5]);
}

NNH_TEST(ui_nav_an_explicit_neighbour_wins) {
    Column m;
    m.ui.set_neighbour(m.a, UiDir::Down, m.c);
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.c);
}

NNH_TEST(ui_nav_neighbours_cross_to_the_other_root) {
    Column m;
    const UiWidget info = m.ui.button(m.ui.root(Screen::Top), {.text = "info"});
    tap(m.ui, kLeft);
    CHECK(m.ui.focused() == m.a && ui_test::has(m.ui, UiResultKind::Blocked)); // never automatic
    m.ui.set_neighbour(m.a, UiDir::Left, info);
    tap(m.ui, kLeft);
    CHECK(m.ui.focused() == info);
}

NNH_TEST(ui_nav_sliders_and_choices_take_left_right_and_pass_up_down) {
    Ui ui = ui_test::make({.texts = 3});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget s = ui.slider(box, {.text = "s", .value = 5});
    static const nnh::UiText opts[3] = {"x", "y", "z"};
    const UiWidget c = ui.choice(box, {.text = "c", .options = opts, .count = 3});
    ui_test::settle(ui);
    tap(ui, kRight);
    CHECK(ui.focused() == s && ui.value(s) == 6);
    tap(ui, kDown);
    CHECK(ui.focused() == c);
    tap(ui, kLeft);
    CHECK(ui.focused() == c && ui.value(c) == 2); // wraps
}

NNH_TEST(ui_nav_toggles_take_left_right_only_when_it_changes_them) {
    Ui ui = ui_test::make();
    const UiWidget row = ui.hbox(ui.root(Screen::Bottom));
    const UiWidget t = ui.toggle(row, {.text = "t"});
    const UiWidget b = ui.button(row, {.text = "b"});
    ui_test::settle(ui);
    tap(ui, kLeft); // already off: passes on (the hbox wraps to b)
    CHECK(ui.focused() == b);
    ui.focus(t);
    tap(ui, kRight); // off → on: consumed
    CHECK(ui.focused() == t && ui.value(t) == 1);
    tap(ui, kRight); // already on: passes
    CHECK(ui.focused() == b);
}

NNH_TEST(ui_nav_the_geometric_rule_crosses_boxes) {
    Ui ui = ui_test::make();
    const UiWidget left = ui.vbox(ui.root(Screen::Bottom), {.layout = {.place = {.x = 10, .y = 10}}});
    const UiWidget right = ui.vbox(ui.root(Screen::Bottom), {.layout = {.place = {.x = 200, .y = 10}}});
    ui.button(left, {.text = "l1"});
    const UiWidget l2 = ui.button(left, {.text = "l2"});
    ui.button(right, {.text = "r1"});
    const UiWidget r2 = ui.button(right, {.text = "r2"});
    ui_test::settle(ui);
    ui.focus(l2);
    tap(ui, kRight);
    CHECK(ui.focused() == r2); // overlapping row first
    tap(ui, kRight);
    CHECK(ui.focused() == r2 && ui_test::has(ui, UiResultKind::Blocked));
}

NNH_TEST(ui_nav_a_blocked_move_keeps_focus_and_reports_once) {
    Ui ui = ui_test::make();
    const UiWidget only = ui.button(ui.root(Screen::Bottom), {.text = "only"});
    ui_test::settle(ui);
    tap(ui, kUp);
    CHECK(ui.focused() == only && ui_test::count(ui, UiResultKind::Blocked) == 1 && ui.results().size == 1);
}

NNH_TEST(ui_nav_a_direction_without_focus_enters_the_first_root) {
    Column m;
    m.ui.focus(nnh::UiWidget{});
    tap(m.ui, kDown);
    CHECK(m.ui.focused() == m.a);
}
