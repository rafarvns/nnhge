// Values (spec 028 T4.3, FR-6): sliders step and clamp (repeats too; blocked at a limit), toggles flip on confirm and
// set by left/right, choices step with wrap per widget and on confirm; set_value reports nothing.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiResultKind;
using nnh::UiWidget;
using ui_test::hold;
using ui_test::kConfirm;
using ui_test::kLeft;
using ui_test::kRight;
using ui_test::press;
using ui_test::tap;

namespace {
const nnh::UiText kOpts[3] = {"Easy", "Normal", "Hard"};
}

NNH_TEST(ui_values_slider_steps_and_reports) {
    Ui ui = ui_test::make();
    const UiWidget s = ui.slider(ui.root(Screen::Bottom), {.text = "Music", .min = 0, .max = 100, .step = 5, .value = 50});
    ui_test::settle(ui);
    tap(ui, kRight);
    CHECK(ui.value(s) == 55 && ui_test::count(ui, UiResultKind::ValueChanged) == 1);
    CHECK(ui.results().data[0].value == 55);
    tap(ui, kLeft);
    tap(ui, kLeft);
    CHECK(ui.value(s) == 45);
}

NNH_TEST(ui_values_slider_held_repeats_and_clamps) {
    Ui ui = ui_test::make();
    const UiWidget s = ui.slider(ui.root(Screen::Bottom), {.min = 0, .max = 10, .value = 7});
    ui_test::settle(ui);
    ui.update(press(kLeft));
    for (int f = 1; f < 40; ++f) ui.update(hold(kLeft)); // press, 24, 30, 36
    CHECK(ui.value(s) == 3);
    for (int f = 0; f < 40; ++f) ui.update(hold(kLeft)); // 42, 48, 54, 60(0), 66: blocked, 72...
    CHECK(ui.value(s) == 0);
    ui.update(UiInput{});
    tap(ui, kLeft);
    CHECK(ui.value(s) == 0 && ui_test::has(ui, UiResultKind::Blocked));
}

NNH_TEST(ui_values_toggle_flips_on_confirm) {
    Ui ui = ui_test::make();
    const UiWidget t = ui.toggle(ui.root(Screen::Bottom), {.text = "Invert"});
    ui_test::settle(ui);
    tap(ui, kConfirm);
    CHECK(ui.value(t) == 1 && ui.changed(nnh::UiKey{}) == false && ui_test::has(ui, UiResultKind::ValueChanged));
    CHECK(ui.state(t) == nnh::UiState::Pressed);
    tap(ui, kConfirm);
    CHECK(ui.value(t) == 0);
}

NNH_TEST(ui_values_choice_wraps_or_blocks) {
    Ui ui = ui_test::make({.texts = 6});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget w = ui.choice(box, {.text = "A", .options = kOpts, .count = 3, .index = 2});
    const UiWidget n = ui.choice(box, {.text = "B", .options = kOpts, .count = 3, .index = 2, .wrap = false});
    ui_test::settle(ui);
    tap(ui, kRight);
    CHECK(ui.value(w) == 0);
    ui.focus(n);
    tap(ui, kRight);
    CHECK(ui.value(n) == 2 && ui_test::has(ui, UiResultKind::Blocked));
    tap(ui, kLeft);
    CHECK(ui.value(n) == 1);
    tap(ui, kConfirm); // next, wrapping on confirm
    tap(ui, kConfirm);
    CHECK(ui.value(n) == 0);
}

NNH_TEST(ui_values_set_value_reports_nothing_and_clamps) {
    Ui ui = ui_test::make({.texts = 3});
    const UiWidget s = ui.slider(ui.root(Screen::Bottom), {.min = 0, .max = 10});
    const UiWidget c = ui.choice(ui.root(Screen::Bottom), {.options = kOpts, .count = 3});
    const UiWidget p = ui.progress(ui.root(Screen::Bottom), {.max = 50});
    const UiWidget t = ui.toggle(ui.root(Screen::Bottom), {.text = "t"});
    ui_test::settle(ui);
    ui.set_value(s, 99);
    ui.set_value(c, -4);
    ui.set_value(p, 30);
    ui.set_value(t, 7);
    CHECK(ui.value(s) == 10 && ui.value(c) == 0 && ui.value(p) == 30 && ui.value(t) == 1);
    ui.update(UiInput{});
    CHECK(ui.results().size == 0);
    ui.set_progress_max(p, 20);
    CHECK(ui.value(p) == 20);
}

NNH_TEST(ui_values_button_activation_and_pressed_look_for_six_frames) {
    Ui ui = ui_test::make();
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "Go"});
    ui_test::settle(ui);
    ui.update(press(kConfirm));
    CHECK(ui.activated(b) && ui.state(b) == nnh::UiState::Pressed);
    for (int f = 1; f < nnh::kUiPressedFrames; ++f) {
        ui.update(UiInput{});
        CHECK(ui.state(b) == nnh::UiState::Pressed);
    }
    ui.update(UiInput{});
    CHECK(ui.state(b) == nnh::UiState::Focused);
}

NNH_TEST(ui_values_inert_widgets_are_refused) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom), {.disabled_focusable = true});
    const UiWidget online = ui.button(box, {.text = "Online"});
    ui.set_enabled(online, false);
    ui_test::settle(ui);
    tap(ui, kConfirm);
    CHECK(!ui.activated(online) && ui_test::count(ui, UiResultKind::Refused) == 1);
    CHECK(ui.results().data[0].widget == online);
}
