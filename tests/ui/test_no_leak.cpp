// No leaking presses (spec 028 T4.2, FR-22): when a UI becomes active — its first update, after reset_input(), after
// a gap in frame_count() (a scene above it was popped), after a modal opens — controls already held are ignored until
// released.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;
using ui_test::hold;
using ui_test::kConfirm;
using ui_test::press;

namespace {

struct One {
    Ui ui = ui_test::make();
    UiWidget a;
    One() { a = ui.button(ui.root(Screen::Bottom), {.text = "a"}); }
};

} // namespace

NNH_TEST(ui_no_leak_a_confirm_held_at_the_first_update) {
    One m;
    m.ui.update(press(kConfirm)); // the A that opened this menu
    CHECK(!m.ui.activated(m.a));
    m.ui.update(hold(kConfirm));
    CHECK(!m.ui.activated(m.a));
    m.ui.update(UiInput{});
    m.ui.update(press(kConfirm));
    CHECK(m.ui.activated(m.a));
}

NNH_TEST(ui_no_leak_after_reset_input) {
    One m;
    ui_test::settle(m.ui);
    m.ui.reset_input();
    m.ui.update(press(kConfirm));
    CHECK(!m.ui.activated(m.a));
}

NNH_TEST(ui_no_leak_a_touch_held_at_activation_is_ignored) {
    One m;
    const nnh::UiRect r = m.ui.rect(m.a);
    m.ui.update(ui_test::touch_hold(r.x + 1, r.y + 1));
    m.ui.update(ui_test::touch_end(r.x + 1, r.y + 1));
    CHECK(!m.ui.activated(m.a));
    m.ui.update(ui_test::touch_start(r.x + 1, r.y + 1));
    m.ui.update(ui_test::touch_end(r.x + 1, r.y + 1));
    CHECK(m.ui.activated(m.a));
}

NNH_TEST(ui_no_leak_after_a_frame_gap) {
    ui_test::restart();
    {
        One m;
        CHECK(nnh::frame());
        m.ui.update(UiInput{});
        CHECK(nnh::frame());
        m.ui.update(UiInput{}); // updated every frame: no activation
        m.ui.update(press(kConfirm));
        CHECK(m.ui.activated(m.a));
        CHECK(nnh::frame());
        CHECK(nnh::frame());
        CHECK(nnh::frame()); // not updated for two frames (a scene above it)
        m.ui.update(press(kConfirm));
        CHECK(!m.ui.activated(m.a));
    }
    nnh::shutdown();
}

NNH_TEST(ui_no_leak_several_updates_in_one_frame_are_no_gap) {
    One m;
    ui_test::settle(m.ui);
    for (int i = 0; i < 5; ++i) m.ui.update(UiInput{});
    m.ui.update(press(kConfirm));
    CHECK(m.ui.activated(m.a));
}
