// Modals (spec 028 T3.5, FR-18): navigation, confirm and touch reach only the open modal; closing gives the focus
// back; nesting to 8, a 9th asserts; a confirm held when it opened is ignored; outside touches ignored or cancelling.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiResultKind;
using nnh::UiWidget;
using ui_test::kConfirm;
using ui_test::kDown;
using ui_test::tap;

namespace {

struct Scene {
    Ui ui = ui_test::make({.widgets = 64});
    UiWidget menu, m1, m2, dialog, yes, no;
    Scene() {
        menu = ui.vbox(ui.root(Screen::Bottom));
        m1 = ui.button(menu, {.text = "m1"});
        m2 = ui.button(menu, {.text = "m2"});
        dialog = ui.hbox(ui.root(Screen::Bottom), {.layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
        yes = ui.button(dialog, {.text = "Yes"});
        no = ui.button(dialog, {.text = "No"});
        ui.set_visible(dialog, false);
        ui_test::settle(ui);
    }
    void open() {
        ui.set_visible(dialog, true);
        ui.open_modal(dialog);
    }
};

Ui* g_ui = nullptr;
UiWidget g_box;

} // namespace

NNH_TEST(ui_modal_takes_the_focus_and_scopes_navigation) {
    Scene s;
    tap(s.ui, kDown);
    CHECK(s.ui.focused() == s.m2);
    s.open();
    CHECK(s.ui.focused() == s.yes && s.ui.modal_depth() == 1);
    for (int i = 0; i < 5; ++i) {
        tap(s.ui, kDown); // no axis in the hbox, nothing below inside the modal: blocked
        CHECK(s.ui.focused() == s.yes || s.ui.focused() == s.no);
    }
    tap(s.ui, ui_test::kRight);
    CHECK(s.ui.focused() == s.no);
}

NNH_TEST(ui_modal_close_gives_the_focus_back) {
    Scene s;
    tap(s.ui, kDown);
    s.open();
    s.ui.close_modal();
    CHECK(s.ui.focused() == s.m2 && s.ui.modal_depth() == 0);
}

NNH_TEST(ui_modal_a_confirm_held_when_it_opened_is_ignored) {
    Scene s;
    s.ui.update(ui_test::press(kConfirm));
    CHECK(s.ui.activated(s.m1));
    s.open(); // e.g. from the activation
    s.ui.update(ui_test::hold(kConfirm));
    s.ui.update(ui_test::press(kConfirm)); // a "press" while still held from before: ignored until released
    CHECK(s.ui.results().size == 0);
    s.ui.update(UiInput{});
    s.ui.update(ui_test::press(kConfirm));
    CHECK(s.ui.activated(s.yes));
}

NNH_TEST(ui_modal_nests_to_eight_and_a_ninth_asserts) {
    Ui ui = ui_test::make({.widgets = 64});
    g_ui = &ui;
    UiWidget boxes[9];
    for (UiWidget& b : boxes) {
        b = ui.vbox(ui.root(Screen::Bottom));
        ui.button(b, {.text = "x"});
    }
    for (int i = 0; i < 8; ++i) ui.open_modal(boxes[i]);
    CHECK(ui.modal_depth() == 8);
    g_box = boxes[8];
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->open_modal(g_box); }));
    CHECK(ui.modal_depth() == 8);
    for (int i = 0; i < 8; ++i) ui.close_modal();
    CHECK(ui.modal_depth() == 0);
}

NNH_TEST(ui_modal_touch_outside_is_ignored) {
    Scene s;
    s.open();
    s.ui.update(UiInput{}); // the activation that follows an opening
    const nnh::UiRect m1 = s.ui.rect(s.m1);
    s.ui.update(ui_test::touch_start(m1.x + 2, m1.y + 2));
    s.ui.update(ui_test::touch_end(m1.x + 2, m1.y + 2));
    CHECK(!s.ui.activated(s.m1) && s.ui.modal_depth() == 1 && s.ui.focused() == s.yes);
}

NNH_TEST(ui_modal_touch_outside_can_cancel) {
    Scene s;
    s.ui.set_visible(s.dialog, true);
    s.ui.open_modal(s.dialog, {.outside_touch_cancels = true});
    s.ui.update(UiInput{});
    s.ui.update(ui_test::touch_start(1, 1));
    CHECK(s.ui.cancelled() && s.ui.modal_depth() == 0);
}

NNH_TEST(ui_modal_cancel_closes_it) {
    Scene s;
    s.open();
    s.ui.update(UiInput{});
    tap(s.ui, ui_test::kCancel);
    CHECK(s.ui.cancelled() && s.ui.modal_depth() == 0 && s.ui.focused() == s.m1);
}

NNH_TEST(ui_modal_only_a_container_opens) {
    Scene s;
    g_ui = &s.ui;
    g_box = s.m1;
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->open_modal(g_box); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->close_modal(); }));
}
