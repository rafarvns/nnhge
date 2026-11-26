// Engine controls per tick (spec 030 T3.3, FR-8, FR-10): edges, repeat timing 24/6, touch down/held/up.

#include "debug/dev_controls.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"
#include "test.hpp"

using nnh::detail::DevControls;
using nnh::detail::DevInput;

namespace {

constexpr std::uint32_t bit(nnh::Button b) { return static_cast<std::uint32_t>(b); }

nnh::platform::InputSample buttons(std::uint32_t mask) {
    nnh::platform::InputSample s;
    s.buttons = mask;
    return s;
}

nnh::platform::InputSample touch(int x, int y) {
    nnh::platform::InputSample s;
    s.touching = true;
    s.touch_x = static_cast<std::int16_t>(x);
    s.touch_y = static_cast<std::int16_t>(y);
    return s;
}

} // namespace

NNH_TEST(dev_controls_edges) {
    DevControls c;
    const std::uint32_t a = bit(nnh::Button::A);
    DevInput in = c.update(buttons(a));
    CHECK(in.held == a && in.pressed == a && in.released == 0 && in.repeated == a);
    in = c.update(buttons(a));
    CHECK(in.held == a && in.pressed == 0 && in.repeated == 0);
    in = c.update(buttons(0));
    CHECK(in.held == 0 && in.released == a);
}

NNH_TEST(dev_controls_repeat_after_24_ticks_then_every_6) {
    DevControls c;
    const std::uint32_t down = bit(nnh::Button::Down);
    int repeats[80] = {};
    for (int t = 0; t < 80; ++t) repeats[t] = (c.update(buttons(down)).repeated & down) != 0 ? 1 : 0;
    CHECK(repeats[0] == 1); // the press
    for (int t = 1; t < 24; ++t) CHECK(repeats[t] == 0);
    CHECK(repeats[24] == 1);
    for (int t = 25; t < 30; ++t) CHECK(repeats[t] == 0);
    CHECK(repeats[30] == 1 && repeats[36] == 1 && repeats[42] == 1);
}

NNH_TEST(dev_controls_repeat_restarts_after_a_release) {
    DevControls c;
    const std::uint32_t r = bit(nnh::Button::Right);
    for (int t = 0; t < 30; ++t) c.update(buttons(r));
    c.update(buttons(0));
    CHECK((c.update(buttons(r)).pressed & r) != 0);
    for (int t = 1; t < 24; ++t) CHECK((c.update(buttons(r)).repeated & r) == 0);
}

NNH_TEST(dev_controls_touch_down_held_up) {
    DevControls c;
    DevInput in = c.update(touch(10, 20));
    CHECK(in.touching && in.touch_started && !in.touch_ended && in.touch_x == 10 && in.touch_y == 20);
    in = c.update(touch(12, 22));
    CHECK(in.touching && !in.touch_started && in.touch_x == 12);
    in = c.update(nnh::platform::InputSample{});
    CHECK(!in.touching && in.touch_ended && in.touch_x == 12 && in.touch_y == 22);
}

NNH_TEST(dev_controls_reset_forgets_held_buttons) {
    DevControls c;
    const std::uint32_t a = bit(nnh::Button::A);
    c.update(buttons(a));
    c.reset();
    CHECK(c.update(buttons(a)).pressed == a);
}
