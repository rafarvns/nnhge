// The default input source (spec 028 T4.2, FR-22): the D-Pad, the Circle Pad with hysteresis (0.5 to press, below 0.4
// to let go), A/B (swappable), L/R pages, touch; idle under input::set_blocked and a transition's block.

#include "input/input_block.hpp"
#include "input/stick.hpp"
#include "ui_fixture.hpp"

using nnh::Button;
using nnh::Screen;
using nnh::Ui;
using nnh::UiResultKind;
using nnh::UiWidget;

namespace {

namespace fake = nnh::platform_fake;

struct Column {
    Ui ui = ui_test::make();
    UiWidget a, b, c;
    explicit Column(const nnh::UiInputConfig& in = {}) {
        ui = ui_test::make({.input = in});
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        c = ui.button(box, {.text = "c"});
    }
};

/// The raw platform value that reads `shaped` after the dead zone (input/stick.cpp).
float raw(float shaped) {
    const float dz = nnh::detail::kStickDeadZone;
    return shaped < 0 ? -(dz + -shaped * (1 - dz)) : dz + shaped * (1 - dz);
}

/// One engine frame then the UI's update.
void step(Ui& ui) {
    CHECK(nnh::frame());
    ui.update();
}

} // namespace

NNH_TEST(ui_raw_dpad_moves_the_focus) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        step(m.ui);
        CHECK(m.ui.focused() == m.b);
        step(m.ui); // still held: no new move until the repeat
        CHECK(m.ui.focused() == m.b);
        fake::set_held_buttons(0);
        step(m.ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        step(m.ui);
        CHECK(m.ui.focused() == m.c);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_circle_pad_with_hysteresis) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        fake::set_circle_pad(0, raw(-0.45f)); // below the press threshold
        step(m.ui);
        CHECK(m.ui.focused() == m.a);
        fake::set_circle_pad(0, raw(-0.6f)); // pressed
        step(m.ui);
        CHECK(m.ui.focused() == m.b);
        fake::set_circle_pad(0, raw(-0.45f)); // still held (above the release threshold)
        step(m.ui);
        fake::set_circle_pad(0, raw(-0.6f));
        step(m.ui);
        CHECK(m.ui.focused() == m.b); // not a new press
        fake::set_circle_pad(0, raw(-0.3f)); // let go
        step(m.ui);
        fake::set_circle_pad(0, raw(-0.6f));
        step(m.ui);
        CHECK(m.ui.focused() == m.c);
        fake::set_circle_pad(0, 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_circle_pad_can_be_off) {
    ui_test::restart();
    {
        Column m({.circle_pad = false});
        step(m.ui);
        fake::set_circle_pad(0, -1.0f);
        step(m.ui);
        CHECK(m.ui.focused() == m.a);
        fake::set_circle_pad(0, 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_confirm_and_cancel_can_be_swapped) {
    ui_test::restart();
    {
        Column m({.confirm = Button::B, .cancel = Button::A});
        step(m.ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::B));
        step(m.ui);
        CHECK(m.ui.activated(m.a));
        fake::set_held_buttons(0);
        step(m.ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A));
        step(m.ui);
        CHECK(m.ui.cancelled() && !m.ui.activated(m.a));
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_l_and_r_page) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::R));
        step(m.ui);
        CHECK(ui_test::has(m.ui, UiResultKind::Page) && m.ui.results().data[0].value == 1);
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_touch_from_the_bottom_screen) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        const nnh::UiRect r = m.ui.rect(m.c);
        fake::set_touch(r.x + 3, r.y + 3);
        step(m.ui);
        CHECK(m.ui.focused() == m.c);
        fake::release_touch();
        step(m.ui);
        CHECK(m.ui.activated(m.c));
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_reads_idle_while_input_is_blocked) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        nnh::input::set_blocked(true);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        step(m.ui);
        step(m.ui);
        CHECK(m.ui.focused() == m.a);
        fake::set_held_buttons(0);
        nnh::input::set_blocked(false);
    }
    nnh::shutdown();
}

NNH_TEST(ui_raw_reads_idle_during_a_transition_block) {
    ui_test::restart();
    {
        Column m;
        step(m.ui);
        nnh::detail::input_block(nnh::detail::InputBlocker::Transition, true);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A));
        step(m.ui);
        CHECK(!m.ui.activated(m.a));
        nnh::detail::input_block(nnh::detail::InputBlocker::Transition, false);
        step(m.ui);
        CHECK(!m.ui.activated(m.a)); // held through the block: filtered until released
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}
