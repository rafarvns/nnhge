// The actions adapter (spec 028 T4.7, FR-22): Ui::use_actions reads the game's spec 017 actions — a remapped
// confirm, a vector action as four directions — and unbound actions read idle.

#include "nnh/actions.hpp"
#include "ui_fixture.hpp"

using nnh::Button;
using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;

namespace {

namespace fake = nnh::platform_fake;

enum class Act { Confirm, Back, Move, Up, Down };

nnh::ActionSet make_set() {
    nnh::ActionSet set({.actions = 5});
    CHECK(set.add(Act::Confirm, {.name = "confirm", .bindings = {{.source = nnh::Source::button(Button::X)}}}));
    CHECK(set.add(Act::Back, {.name = "back", .bindings = {{.source = nnh::Source::button(Button::Y)}}}));
    CHECK(set.add(Act::Move, {.name = "move", .kind = nnh::ActionKind::Vector,
                        .bindings = {{.source = nnh::Source::dpad()}}}));
    CHECK(set.add(Act::Up, {.name = "up", .bindings = {{.source = nnh::Source::button(Button::Up)}}}));
    CHECK(set.add(Act::Down, {.name = "down"})); // unbound
    return set;
}

void step(Ui& ui) {
    CHECK(nnh::frame());
    ui.update();
}

} // namespace

NNH_TEST(ui_actions_a_remapped_confirm_activates) {
    ui_test::restart();
    {
        nnh::ActionSet set = make_set();
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        const UiWidget a = ui.button(box, {.text = "a"});
        ui.use_actions(set, {.up = Act::Up, .down = Act::Down, .move = Act::Move, .confirm = Act::Confirm,
                             .cancel = Act::Back});
        step(ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A)); // A isn't confirm any more
        step(ui);
        CHECK(!ui.activated(a));
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::X));
        step(ui);
        CHECK(ui.activated(a));
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Y));
        step(ui);
        CHECK(ui.cancelled());
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_actions_a_vector_action_moves_in_four_directions) {
    ui_test::restart();
    {
        nnh::ActionSet set = make_set();
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        ui.button(box, {.text = "a"});
        const UiWidget b = ui.button(box, {.text = "b"});
        ui.use_actions(set, {.move = Act::Move, .confirm = Act::Confirm});
        step(ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down)); // the vector reads (0, -1)
        step(ui);
        step(ui);
        CHECK(set.vector(Act::Move).y < -0.5f);
        CHECK(ui.focused() == b);
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_actions_unbound_actions_read_idle) {
    ui_test::restart();
    {
        nnh::ActionSet set = make_set();
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        const UiWidget a = ui.button(box, {.text = "a"});
        ui.button(box, {.text = "b"});
        ui.use_actions(set, {.down = Act::Down});
        step(ui);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        step(ui);
        step(ui);
        CHECK(ui.focused() == a);
        fake::set_held_buttons(0);
    }
    nnh::shutdown();
}
