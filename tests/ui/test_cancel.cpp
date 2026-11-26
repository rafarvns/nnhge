// Cancel (spec 028 T4.5, FR-22): the innermost container's back widget is activated, else an open modal with
// cancel_closes closes; Cancelled is reported in every case.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiResultKind;
using nnh::UiWidget;
using ui_test::K;
using ui_test::kCancel;
using ui_test::tap;

NNH_TEST(ui_cancel_activates_the_back_widget) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "Music"});
    const UiWidget back = ui.button(box, {.text = "Back", .key = K::Back});
    ui.set_back(box, back);
    ui_test::settle(ui);
    tap(ui, kCancel);
    CHECK(ui.cancelled() && ui.activated(K::Back));
    CHECK(ui.results().data[0].kind == UiResultKind::Activated);
}

NNH_TEST(ui_cancel_uses_the_innermost_back) {
    Ui ui = ui_test::make();
    const UiWidget outer = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget outer_back = ui.button(outer, {.text = "Quit"});
    const UiWidget inner = ui.vbox(outer);
    ui.button(inner, {.text = "x"});
    const UiWidget inner_back = ui.button(inner, {.text = "Back"});
    ui.set_back(outer, outer_back);
    ui.set_back(inner, inner_back);
    ui_test::settle(ui);
    tap(ui, ui_test::kDown); // into inner
    tap(ui, kCancel);
    CHECK(ui.activated(inner_back) && !ui.activated(outer_back));
}

NNH_TEST(ui_cancel_without_a_back_widget_is_only_reported) {
    Ui ui = ui_test::make();
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b", .key = K::B});
    ui_test::settle(ui);
    tap(ui, kCancel);
    CHECK(ui.cancelled() && ui.results().size == 1 && ui.results().data[0].key == nnh::UiKey(K::B));
    CHECK(ui.focused() == b);
}

NNH_TEST(ui_cancel_closes_a_modal_unless_told_not_to) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.button(ui.root(Screen::Bottom), {.text = "a"});
    const UiWidget m = ui.vbox(ui.root(Screen::Bottom));
    ui.button(m, {.text = "in"});
    ui_test::settle(ui);
    ui.open_modal(m, {.cancel_closes = false});
    ui.update(nnh::UiInput{});
    tap(ui, kCancel);
    CHECK(ui.cancelled() && ui.modal_depth() == 1);
    ui.close_modal();
    ui.open_modal(m);
    ui.update(nnh::UiInput{});
    tap(ui, kCancel);
    CHECK(ui.modal_depth() == 0 && ui.focused() == a);
}
