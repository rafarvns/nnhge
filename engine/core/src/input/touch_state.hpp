#pragma once
// Touch edge detection and last point. Pure logic, unit tested in tests/input/test_touch_state.cpp.

#include "nnh/input.hpp"

namespace nnh::detail {

struct TouchState {
    bool touching = false;
    bool previous = false;
    TouchPoint position; ///< current point (clamped to the bottom screen), or the last one while not touching

    void update(bool now, int x, int y) {
        previous = touching;
        touching = now;
        if (now) position = TouchPoint{clamp(x, 319), clamp(y, 239)};
    }
    bool started() const { return touching && !previous; }
    bool ended() const { return previous && !touching; }

private:
    static int clamp(int v, int max) { return v < 0 ? 0 : (v > max ? max : v); }
};

} // namespace nnh::detail
