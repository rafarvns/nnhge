// The input block (input_block.hpp) and the public input::set_blocked / input::blocked.

#include "input/input_block.hpp"

#include "input/input_internal.hpp"
#include "input/stick.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"

namespace nnh {
namespace {

bool moved(float x, float y) {
    const Stick s = detail::shape_stick(x, y);
    return s.x != 0 || s.y != 0;
}

detail::InputBlock g_block;

void run_filter(const platform::InputSample& raw) {
    platform::InputSample sample = raw;
    const bool idle = g_block.filter(sample);
    if (!g_block.needed()) detail::g_input_filter = nullptr; // nothing left to filter: back to the plain path
    detail::input_apply(sample);
    if (idle) detail::input_forget_edges(); // blocked: idle without edges (no release of what was held)
}

} // namespace

void detail::InputBlock::set(InputBlocker who, bool on) {
    const auto bit = static_cast<std::uint8_t>(who);
    blockers = static_cast<std::uint8_t>(on ? blockers | bit : blockers & ~bit);
}

bool detail::InputBlock::filter(platform::InputSample& s) {
    const bool circle_moved = moved(s.circle_x, s.circle_y);
    const bool cstick_moved = moved(s.cstick_x, s.cstick_y);
    if (blockers != 0) {
        filtered |= s.buttons;
        touch = touch || s.touching;
        circle = circle || circle_moved;
        cstick = cstick || cstick_moved;
        s = platform::InputSample{};
        return true;
    }
    filtered &= s.buttons; // released buttons drop out
    s.buttons &= ~filtered;
    if (touch) {
        touch = s.touching;
        s.touching = false;
    }
    if (circle) {
        circle = circle_moved;
        if (circle_moved) s.circle_x = s.circle_y = 0;
    }
    if (cstick) {
        cstick = cstick_moved;
        if (cstick_moved) s.cstick_x = s.cstick_y = 0;
    }
    return false;
}

void detail::input_block(InputBlocker who, bool on) {
    if (g_input_filter == nullptr) {
        if (!on) return;
        g_block = InputBlock{}; // a block left from before init() (the filter pointer is reset there)
    }
    g_block.set(who, on);
    g_input_filter = g_block.needed() ? &run_filter : nullptr;
}

void input::set_blocked(bool blocked) { detail::input_block(detail::InputBlocker::Game, blocked); }

bool input::blocked() { return detail::g_input_filter != nullptr && g_block.blocking(); }

} // namespace nnh
