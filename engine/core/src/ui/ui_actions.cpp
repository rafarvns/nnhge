// The spec 017 actions adapter (spec 028 FR-22): Ui::use_actions makes the UI read its controls from the game's
// ActionSet — buttons from button actions, the four directions also from a vector action past 0.5 (let go below 0.4),
// the touch screen raw. The only UI file that includes <nnh/actions.hpp>: linked only by games that call use_actions.

#include "nnh/actions.hpp"
#include "nnh/input.hpp"
#include "ui/ui_block.hpp"

namespace nnh {
namespace detail {
namespace {

bool held(const ActionSet& set, UiActionRef a) {
    return a.index != 0xFFFF && set.held(ActionId(static_cast<int>(a.index)));
}

bool past(bool& was, float v) {
    was = was ? v >= 0.4f : v >= 0.5f;
    return was;
}

void adapter(UiBlock& b, UiInput& out) {
    const ActionSet& set = *b.actions;
    const UiActionMap& m = b.action_map;
    std::uint8_t h = 0;
    const UiActionRef controls[8] = {m.up, m.down, m.left, m.right, m.confirm, m.cancel, m.page_back, m.page_forward};
    for (int c = 0; c < 8; ++c) {
        if (held(set, controls[c])) h |= static_cast<std::uint8_t>(1u << c);
    }
    if (m.move.index != 0xFFFF) {
        const Vec2 v = set.vector(ActionId(static_cast<int>(m.move.index)));
        if (past(b.circle[0], v.y)) h |= ui_bit(UiControl::Up);
        if (past(b.circle[1], -v.y)) h |= ui_bit(UiControl::Down);
        if (past(b.circle[2], -v.x)) h |= ui_bit(UiControl::Left);
        if (past(b.circle[3], v.x)) h |= ui_bit(UiControl::Right);
    }
    out.held = h;
    out.pressed = static_cast<std::uint8_t>(h & ~b.raw_held);
    b.raw_held = h;
    if (m.touch) {
        out.touching = input::touching();
        out.touch_started = input::touch_started();
        out.touch_ended = input::touch_ended();
        const TouchPoint p = input::touch_position();
        out.touch_x = static_cast<std::int16_t>(p.x);
        out.touch_y = static_cast<std::int16_t>(p.y);
    }
}

} // namespace
} // namespace detail

void Ui::use_actions(const ActionSet& set, const UiActionMap& map) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    b->actions = &set;
    b->action_map = map;
    b->adapter = &detail::adapter;
    b->source = UiInputSource{};
}

} // namespace nnh
