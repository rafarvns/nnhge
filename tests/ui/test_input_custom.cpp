// The game's own input (spec 028 T4.5, FR-22): update(input), a source function, back to raw with an empty source;
// repeat_delay 0 takes the input's own repeat bits.

#include "nnh/input.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;

namespace {

nnh::UiInput g_next;

struct Column {
    Ui ui;
    UiWidget a, b, c;
    explicit Column(const nnh::UiDesc& d = {}) : ui(ui_test::make(d)) {
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        c = ui.button(box, {.text = "c"});
    }
};

} // namespace

NNH_TEST(ui_custom_a_source_function_feeds_update) {
    ui_test::restart();
    {
        Column m;
        m.ui.set_input_source([](UiInput& out) { out = g_next; });
        g_next = UiInput{};
        CHECK(nnh::frame());
        m.ui.update();
        g_next = ui_test::press(ui_test::kDown);
        CHECK(nnh::frame());
        m.ui.update();
        CHECK(m.ui.focused() == m.b);
        // back to raw input: the D-Pad again
        m.ui.set_input_source({});
        nnh::platform_fake::set_held_buttons(static_cast<std::uint32_t>(nnh::Button::Down));
        CHECK(nnh::frame());
        m.ui.update();
        CHECK(m.ui.focused() == m.c);
        nnh::platform_fake::set_held_buttons(0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_custom_update_with_a_value) {
    Column m;
    ui_test::settle(m.ui);
    m.ui.update(ui_test::press(ui_test::kDown));
    CHECK(m.ui.focused() == m.b);
}

NNH_TEST(ui_custom_repeat_delay_0_takes_the_given_repeats) {
    Column m({.input = {.repeat_delay = 0}});
    ui_test::settle(m.ui);
    m.ui.update(ui_test::press(ui_test::kDown));
    UiInput held = ui_test::hold(ui_test::kDown);
    m.ui.update(held);
    CHECK(m.ui.focused() == m.b);
    held.repeated = ui_test::kDown;
    m.ui.update(held);
    CHECK(m.ui.focused() == m.c);
}
