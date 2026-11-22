#include "controls.hpp"

using nnh::ActionKind;
using nnh::AnalogStick;
using nnh::Binding;
using nnh::Button;
using nnh::Source;

namespace {
Binding button(Button b) { return {Source::button(b)}; }
} // namespace

bool declare_controls(nnh::ActionSet& set) {
    bool ok = set.add_context(Ctx::Play, "play");
    ok = set.add_context(Ctx::Menu, "menu", false) && ok;
    ok = set.add(Act::Move, {.name = "move",
                             .kind = ActionKind::Vector,
                             .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}) && ok;
    ok = set.add(Act::Jump, {.name = "jump", .bindings = {button(Button::A)}}) && ok;
    ok = set.add(Act::Attack, {.name = "attack",
                               .bindings = {button(Button::B), {Source::touch(kTouchX, kTouchY, kTouchW, kTouchH)}}}) &&
         ok;
    ok = set.add(Act::Aim, {.name = "aim",
                            .kind = ActionKind::Axis,
                            .bindings = {{Source::stick_axis(AnalogStick::CStick, nnh::StickAxis::X)},
                                         {.source = Source::buttons(Button::L, Button::R),
                                          .fallback = true,
                                          .fallback_for = nnh::Capability::CStick}}}) && ok;
    ok = set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}) &&
         ok;
    const nnh::Repeat menu_repeat{.delay = 24, .interval = 6};
    ok = set.add(Act::Up, {.name = "menu_up", .context = Ctx::Menu, .bindings = {button(Button::Up)}, .repeat = menu_repeat}) &&
         ok;
    ok = set.add(Act::Down, {.name = "menu_down", .context = Ctx::Menu, .bindings = {button(Button::Down)},
                             .repeat = menu_repeat}) && ok;
    ok = set.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu, .bindings = {button(Button::A)}}) && ok;
    ok = set.add(Act::Back, {.name = "back", .context = Ctx::Menu, .bindings = {button(Button::B)}}) && ok;
    ok = set.add(Act::Reset, {.name = "reset", .context = Ctx::Menu, .bindings = {button(Button::X)}}) && ok;
    ok = set.add(Act::ResetAll, {.name = "reset_all", .context = Ctx::Menu, .bindings = {button(Button::Y)}}) && ok;
    return ok;
}
