// Touch (spec 028 T5.3, FR-19): press shows pressed and focuses; release inside activates; sliding out cancels; toggles
// flip; choice arrows step; sliders follow x; list taps select and activate (tap-to-select option); a drag past 8 px
// scrolls by whole rows and cancels the press; top-root and disabled widgets ignore touches; inert ones refuse; the
// D-Pad continues from the touched widget; the cursor may hide after a touch.

#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiRect;
using nnh::UiResultKind;
using nnh::UiState;
using nnh::UiWidget;
using ui_test::touch_end;
using ui_test::touch_hold;
using ui_test::touch_start;

namespace {

void tap_at(Ui& ui, int x, int y) {
    ui.update(touch_start(x, y));
    ui.update(touch_end(x, y));
}

struct Menu {
    Ui ui = ui_test::make({.texts = 3, .rows = 16});
    UiWidget box, a, b;
    Menu() {
        box = ui.vbox(ui.root(Screen::Bottom), {.layout = {.width = 200}});
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        ui_test::settle(ui);
    }
};

} // namespace

NNH_TEST(ui_touch_press_focuses_and_shows_pressed_release_activates) {
    Menu m;
    const UiRect r = m.ui.rect(m.b);
    m.ui.update(touch_start(r.x + 5, r.y + 5));
    CHECK(m.ui.focused() == m.b && m.ui.state(m.b) == UiState::Pressed && !m.ui.activated(m.b));
    CHECK(ui_test::has(m.ui, UiResultKind::FocusChanged));
    m.ui.update(touch_hold(r.x + 6, r.y + 5));
    m.ui.update(touch_end(r.x + 6, r.y + 5));
    CHECK(m.ui.activated(m.b) && m.ui.state(m.b) == UiState::Pressed); // the pressed look after activation
}

NNH_TEST(ui_touch_sliding_out_cancels) {
    Menu m;
    const UiRect r = m.ui.rect(m.b);
    m.ui.update(touch_start(r.x + 5, r.y + 5));
    m.ui.update(touch_hold(r.x + 5, r.y + 40));
    CHECK(m.ui.state(m.b) == UiState::Focused);
    m.ui.update(touch_hold(r.x + 5, r.y + 5)); // back in: pressed again
    CHECK(m.ui.state(m.b) == UiState::Pressed);
    m.ui.update(touch_end(r.x + 5, r.y + 40));
    CHECK(!m.ui.activated(m.b));
}

NNH_TEST(ui_touch_toggle_and_choice_arrows) {
    Menu m;
    const UiWidget t = m.ui.toggle(m.box, {.text = "t"});
    static const nnh::UiText opts[3] = {"x", "y", "z"};
    const UiWidget c = m.ui.choice(m.box, {.text = "c", .options = opts, .count = 3, .index = 1});
    const UiRect tr = m.ui.rect(t);
    tap_at(m.ui, tr.x + 3, tr.y + 3);
    CHECK(m.ui.value(t) == 1);
    const UiRect cr = m.ui.rect(c);
    // content starts at x + 6: label "c" 8 + 4 → the left arrow at x + 18..26, the right one at the end - 6 - 8
    tap_at(m.ui, cr.x + 20, cr.y + 5);
    CHECK(m.ui.value(c) == 0);
    tap_at(m.ui, cr.x + cr.w - 10, cr.y + 5);
    tap_at(m.ui, cr.x + cr.w - 10, cr.y + 5);
    CHECK(m.ui.value(c) == 2);
    tap_at(m.ui, cr.x + cr.w / 2, cr.y + 5); // the option itself: nothing
    CHECK(m.ui.value(c) == 2);
}

NNH_TEST(ui_touch_slider_follows_x) {
    Menu m;
    const UiWidget s = m.ui.slider(m.box, {.text = "", .min = 0, .max = 10, .show_value = false});
    const UiRect r = m.ui.rect(s); // track: x + 6 .. x + 194 (188 pixels)
    m.ui.update(touch_start(r.x + 6, r.y + 5));
    CHECK(m.ui.value(s) == 0 && m.ui.focused() == s);
    m.ui.update(touch_hold(r.x + 6 + 187, r.y + 5));
    CHECK(m.ui.value(s) == 10);
    m.ui.update(touch_hold(r.x + 6 + 94, r.y + 5)); // 94 / 187 × 10 = 5.03 → 5
    CHECK(m.ui.value(s) == 5);
    m.ui.update(touch_hold(r.x + 6 + 103, r.y + 5)); // 5.5 rounds up: 6
    CHECK(m.ui.value(s) == 6);
    m.ui.update(touch_end(r.x + 6 + 103, r.y + 5));
    CHECK(!m.ui.activated(s));
}

NNH_TEST(ui_touch_list_tap_selects_and_activates) {
    Ui ui = ui_test::make({.rows = 16});
    const UiWidget l = ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}});
    ui_test::settle(ui);
    tap_at(ui, 5, 2 * 12 + 3);
    CHECK(ui.list_info(l).focused == 2 && ui.activated(l) && ui.results().data[1].index == 2);
}

NNH_TEST(ui_touch_list_tap_to_select_then_activate) {
    Ui ui = ui_test::make({.rows = 16});
    const UiWidget l = ui.list(ui.root(Screen::Bottom),
                               {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}, .tap_selects_first = true});
    ui_test::settle(ui);
    tap_at(ui, 5, 2 * 12 + 3);
    CHECK(ui.list_info(l).focused == 2 && !ui.activated(l));
    tap_at(ui, 5, 2 * 12 + 3);
    CHECK(ui.activated(l));
}

NNH_TEST(ui_touch_drag_threshold_8_vs_9_pixels) {
    Ui ui = ui_test::make({.rows = 16});
    const UiWidget l = ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}});
    ui_test::settle(ui);
    ui.update(touch_start(5, 30));
    ui.update(touch_hold(5, 22)); // 8 px: still a press
    ui.update(touch_end(5, 30));
    CHECK(ui.activated(l));
    ui.update(touch_start(5, 30));
    ui.update(touch_hold(5, 21)); // 9 px: a drag, the press is cancelled
    ui.update(touch_end(5, 30));
    CHECK(!ui.activated(l) && ui.list_info(l).first_visible == 0);
}

NNH_TEST(ui_touch_drag_scrolls_by_whole_rows) {
    Ui ui = ui_test::make({.rows = 16});
    const UiWidget l = ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}});
    ui_test::settle(ui);
    ui.update(touch_start(5, 40));
    ui.update(touch_hold(5, 10)); // 30 px up: two rows of 12
    CHECK(ui.list_info(l).first_visible == 2);
    ui.update(touch_hold(5, 46)); // 6 px up from the start: no whole row
    CHECK(ui.list_info(l).first_visible == 0);
    ui.update(touch_hold(5, 15)); // 25 px: two rows (toward zero)
    CHECK(ui.list_info(l).first_visible == 2);
    ui.update(touch_end(5, 52));
}

// Regression (found by ASan on Linux): destroying the list being dragged left the drag pointing at no slot, and the
// next held touch read past the widget block.
NNH_TEST(ui_touch_drag_survives_the_list_being_destroyed) {
    Ui ui = ui_test::make({.rows = 16});
    const UiWidget l = ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}});
    ui_test::settle(ui);
    ui.update(touch_start(5, 40));
    ui.update(touch_hold(5, 10)); // a drag: scrolling
    ui.destroy(l);
    ui.update(touch_hold(5, 5));
    const UiWidget again =
        ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 20, .fill = [](int, nnh::UiRow&) {}});
    ui_test::settle(ui);
    ui.update(touch_hold(5, 0));
    ui.update(touch_end(5, 0));
    CHECK(ui.list_info(again).first_visible == 0 && !ui.activated(again));
}

NNH_TEST(ui_touch_ignores_disabled_and_refuses_inert) {
    Menu m;
    m.ui.set_enabled(m.b, false);
    const UiRect r = m.ui.rect(m.b);
    tap_at(m.ui, r.x + 3, r.y + 3);
    CHECK(m.ui.results().size == 0 && m.ui.focused() == m.a);
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom), {.disabled_focusable = true});
    ui.button(box, {.text = "x"});
    const UiWidget off = ui.button(box, {.text = "off"});
    ui.set_enabled(off, false);
    ui_test::settle(ui);
    const UiRect o = ui.rect(off);
    tap_at(ui, o.x + 3, o.y + 3);
    CHECK(ui_test::has(ui, UiResultKind::Refused) && !ui.activated(off));
}

NNH_TEST(ui_touch_reaches_only_the_bottom_root) {
    Ui ui = ui_test::make();
    const UiWidget top = ui.button(ui.root(Screen::Top), {.text = "top"});
    ui.set_root_visible(Screen::Bottom, true);
    ui_test::settle(ui);
    tap_at(ui, 5, 5); // the top button's spot, but touches are the bottom screen's
    CHECK(!ui.activated(top) && ui.results().size == 0);
}

NNH_TEST(ui_touch_then_the_dpad_continues_from_the_touched_widget) {
    Menu m;
    const UiWidget c = m.ui.button(m.box, {.text = "c"});
    const UiRect r = m.ui.rect(m.b);
    tap_at(m.ui, r.x + 3, r.y + 3);
    ui_test::tap(m.ui, ui_test::kDown);
    CHECK(m.ui.focused() == c);
}

NNH_TEST(ui_touch_can_hide_the_cursor_until_a_direction) {
    Ui ui = ui_test::make({.input = {.hide_cursor_after_touch = true}});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "a"});
    const UiWidget b = ui.button(box, {.text = "b"});
    ui_test::settle(ui);
    const UiRect r = ui.rect(b);
    tap_at(ui, r.x + 3, r.y + 3);
    CHECK(nnh::UiAccess::block(ui)->cursor.hidden);
    ui_test::tap(ui, ui_test::kUp);
    CHECK(!nnh::UiAccess::block(ui)->cursor.hidden);
}
