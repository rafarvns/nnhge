// The default UI input (spec 028 FR-22): the public input queries — the D-Pad, the Circle Pad as directions past a
// threshold with hysteresis (0.5 to press, below 0.4 to let go by default), the confirm, cancel and page buttons of
// the UI's config, the bottom screen's touch. Reads idle under input::set_blocked and a transition's block, as the
// public queries do. Fresh presses are the UI's own edges (held now, not at its previous update).

#include "nnh/input.hpp"
#include "ui/ui_block.hpp"

namespace nnh::detail {
namespace {

/// One Circle Pad direction with hysteresis.
bool circle_dir(bool& was, float v, const UiInputConfig& c) {
    was = was ? v >= c.circle_release : v >= c.circle_press;
    return was;
}

} // namespace

void read_raw_input(UiBlock& b, UiInput& out) {
    const UiInputConfig& c = b.input;
    std::uint8_t held = 0;
    if (input::held(Button::Up)) held |= ui_bit(UiControl::Up);
    if (input::held(Button::Down)) held |= ui_bit(UiControl::Down);
    if (input::held(Button::Left)) held |= ui_bit(UiControl::Left);
    if (input::held(Button::Right)) held |= ui_bit(UiControl::Right);
    if (c.circle_pad) {
        const Stick pad = input::circle_pad();
        if (circle_dir(b.circle[0], pad.y, c)) held |= ui_bit(UiControl::Up);
        if (circle_dir(b.circle[1], -pad.y, c)) held |= ui_bit(UiControl::Down);
        if (circle_dir(b.circle[2], -pad.x, c)) held |= ui_bit(UiControl::Left);
        if (circle_dir(b.circle[3], pad.x, c)) held |= ui_bit(UiControl::Right);
    }
    if (input::held(c.confirm)) held |= ui_bit(UiControl::Confirm);
    if (input::held(c.cancel)) held |= ui_bit(UiControl::Cancel);
    if (input::held(c.page_back)) held |= ui_bit(UiControl::PageBack);
    if (input::held(c.page_forward)) held |= ui_bit(UiControl::PageForward);
    out.held = held;
    out.pressed = static_cast<std::uint8_t>(held & ~b.raw_held);
    out.repeated = 0;
    b.raw_held = held;
    out.touching = input::touching();
    out.touch_started = input::touch_started();
    out.touch_ended = input::touch_ended();
    const TouchPoint p = input::touch_position();
    out.touch_x = static_cast<std::int16_t>(p.x);
    out.touch_y = static_cast<std::int16_t>(p.y);
}

} // namespace nnh::detail
