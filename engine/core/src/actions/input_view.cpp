// The input view (input_view.hpp): built from the public input queries, so actions see exactly what games see.

#include "actions/input_view.hpp"

#include <cmath>

namespace nnh::detail {
namespace {

float positive(float v) { return v > 0.0f ? v : 0.0f; }

void split(InputView& v, int stick, Stick s) {
    v.stick[stick] = s;
    float* p = v.push + stick * 4;
    p[0] = positive(s.y);  // Up
    p[1] = positive(-s.y); // Down
    p[2] = positive(-s.x); // Left
    p[3] = positive(s.x);  // Right
    v.length[stick] = std::sqrt(s.x * s.x + s.y * s.y);
}

} // namespace

InputView make_input_view(std::uint32_t held, Stick circle_pad, Stick c_stick, bool touching, TouchPoint touch) {
    InputView v;
    v.held = held;
    split(v, 0, circle_pad);
    split(v, 1, c_stick);
    v.touching = touching;
    v.touch_x = touch.x;
    v.touch_y = touch.y;
    return v;
}

InputView current_input_view() {
    return make_input_view(input::held_mask(), input::circle_pad(), input::c_stick(), input::touching(),
                           input::touch_position());
}

} // namespace nnh::detail
