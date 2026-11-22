// Menu repeat (spec 017 T2.4, FR-12): the press frame, then after `delay` every `interval` frames.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Down };

ActionSet make(nnh::Repeat repeat) {
    ActionSet set{{.actions = 1}};
    CHECK(set.add(Act::Down, {.name = "down", .bindings = {btn(Button::Down)}, .repeat = repeat}));
    step(set, 0);
    return set;
}

/// Bits of the frames (1 = the press frame) where repeated() was true over `n` held frames.
std::uint32_t pattern(ActionSet& set, int n) {
    std::uint32_t bits = 0;
    for (int i = 1; i <= n; ++i) {
        step(set, bit(Button::Down));
        if (set.repeated(Act::Down)) bits |= 1u << i;
    }
    return bits;
}
} // namespace

NNH_TEST(repeat_fires_on_the_press_then_after_the_delay_every_interval) {
    restart();
    ActionSet set = make({.delay = 4, .interval = 3});
    // Press at 1; repeats where held_frames - 1 >= 4 and (held_frames - 1 - 4) % 3 == 0: 5, 8, 11.
    CHECK(pattern(set, 12) == ((1u << 1) | (1u << 5) | (1u << 8) | (1u << 11)));
    nnh::shutdown();
}

NNH_TEST(repeat_with_interval_zero_fires_only_on_the_press) {
    restart();
    ActionSet set = make({.delay = 2, .interval = 0});
    CHECK(pattern(set, 8) == (1u << 1));
    nnh::shutdown();
}

NNH_TEST(no_repeat_means_repeated_equals_pressed) {
    restart();
    ActionSet set = make({});
    CHECK(pattern(set, 8) == (1u << 1));
    nnh::shutdown();
}

NNH_TEST(repeated_is_false_while_not_held_and_restarts_on_a_new_press) {
    restart();
    ActionSet set = make({.delay = 1, .interval = 1});
    step(set, bit(Button::Down));
    step(set, 0);
    CHECK(!set.repeated(Act::Down));
    step(set, 0);
    CHECK(!set.repeated(Act::Down));
    CHECK(pattern(set, 3) == ((1u << 1) | (1u << 2) | (1u << 3)));
    nnh::shutdown();
}
