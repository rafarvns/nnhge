// Engine controls per tick (see dev_controls.hpp). Pure.

#include "debug/dev_controls.hpp"

#include "nnh/platform/input.hpp"

namespace nnh::detail {

DevInput DevControls::update(const platform::InputSample& s) {
    DevInput in;
    in.held = s.buttons;
    in.pressed = s.buttons & ~previous_;
    in.released = previous_ & ~s.buttons;
    for (int b = 0; b < 16; ++b) {
        const std::uint32_t m = 1u << b;
        if ((s.buttons & m) == 0) {
            ticks_[b] = 0;
            continue;
        }
        const int t = ticks_[b];
        if (t == 0 || (t >= kDevRepeatDelay && (t - kDevRepeatDelay) % kDevRepeatEvery == 0)) in.repeated |= m;
        if (ticks_[b] < 0xFFF0) ++ticks_[b];
    }
    previous_ = s.buttons;
    in.touching = s.touching;
    in.touch_started = s.touching && !touching_;
    in.touch_ended = !s.touching && touching_;
    if (s.touching) {
        last_x_ = s.touch_x;
        last_y_ = s.touch_y;
    }
    in.touch_x = last_x_;
    in.touch_y = last_y_;
    touching_ = s.touching;
    return in;
}

void DevControls::reset() { *this = DevControls{}; }

} // namespace nnh::detail
