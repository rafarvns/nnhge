// The input block's filter, pure (spec 023 FR-12; plan 023 "The input filter"): a blocked sample is idle; controls
// held when the block ends stay filtered until released; the game's and the transitions' blockers are independent.

#include "input/input_block.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/input.hpp"
#include "test.hpp"

using nnh::detail::InputBlock;
using nnh::detail::InputBlocker;
using nnh::platform::InputSample;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t kB = static_cast<std::uint32_t>(nnh::Button::B);

InputSample sample(std::uint32_t buttons, float cx = 0, float cy = 0, bool touching = false) {
    InputSample s;
    s.buttons = buttons;
    s.circle_x = cx;
    s.circle_y = cy;
    s.touching = touching;
    s.touch_x = 100;
    s.touch_y = 50;
    return s;
}

bool idle(const InputSample& s) {
    return s.buttons == 0 && s.circle_x == 0 && s.circle_y == 0 && s.cstick_x == 0 && s.cstick_y == 0 &&
           !s.touching;
}

/// Filters a copy of `s` and returns it.
InputSample run(InputBlock& block, InputSample s, bool* blocking = nullptr) {
    const bool b = block.filter(s);
    if (blocking != nullptr) *blocking = b;
    return s;
}

} // namespace

NNH_TEST(a_blocked_sample_reads_idle) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    InputSample s = sample(kA | kB, 0.9f, -0.9f, true);
    s.cstick_x = 1;
    bool blocking = false;
    CHECK(idle(run(block, s, &blocking)) && blocking);
}

NNH_TEST(an_unblocked_filter_passes_everything_through) {
    InputBlock block;
    bool blocking = true;
    const InputSample s = run(block, sample(kA, 0.5f, 0.5f, true), &blocking);
    CHECK(!blocking && s.buttons == kA && s.circle_x == 0.5f && s.touching);
    CHECK(!block.needed());
}

NNH_TEST(a_button_held_through_the_block_stays_filtered_until_released) {
    InputBlock block;
    block.set(InputBlocker::Transition, true);
    run(block, sample(kA));
    block.set(InputBlocker::Transition, false);
    CHECK(run(block, sample(kA | kB)).buttons == kB); // A still filtered, B pressed after the block passes
    CHECK(block.needed());
    CHECK(run(block, sample(kB)).buttons == kB); // A released: no longer filtered
    CHECK(!block.needed());
    CHECK(run(block, sample(kA | kB)).buttons == (kA | kB)); // a new press of A passes
}

NNH_TEST(a_button_pressed_during_the_block_and_held_after_it_stays_filtered) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    run(block, sample(0));
    run(block, sample(kB)); // pressed while blocked
    block.set(InputBlocker::Game, false);
    CHECK(run(block, sample(kB)).buttons == 0);
    CHECK(run(block, sample(0)).buttons == 0);
    CHECK(run(block, sample(kB)).buttons == kB);
}

NNH_TEST(a_stick_out_of_its_dead_zone_stays_centred_until_released) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    run(block, sample(0, 0.8f, 0));
    block.set(InputBlocker::Game, false);
    InputSample s = run(block, sample(0, 0.8f, 0));
    CHECK(s.circle_x == 0 && s.circle_y == 0);
    s = run(block, sample(0, 0.05f, 0)); // inside the dead zone: released
    CHECK(s.circle_x == 0.05f);
    s = run(block, sample(0, 0.8f, 0));
    CHECK(s.circle_x == 0.8f && !block.needed());
}

NNH_TEST(a_stick_resting_in_its_dead_zone_is_not_filtered) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    run(block, sample(0, 0.05f, 0));
    block.set(InputBlocker::Game, false);
    CHECK(!block.needed());
    CHECK(run(block, sample(0, 0.8f, 0)).circle_x == 0.8f);
}

NNH_TEST(the_c_stick_is_filtered_like_the_circle_pad) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    InputSample s = sample(0);
    s.cstick_y = -1;
    run(block, s);
    block.set(InputBlocker::Game, false);
    CHECK(run(block, s).cstick_y == 0);
    s.cstick_y = 0;
    run(block, s);
    s.cstick_y = -1;
    CHECK(run(block, s).cstick_y == -1);
}

NNH_TEST(a_touch_held_through_the_block_reads_untouched_until_lifted) {
    InputBlock block;
    block.set(InputBlocker::Transition, true);
    run(block, sample(0, 0, 0, true));
    block.set(InputBlocker::Transition, false);
    CHECK(!run(block, sample(0, 0, 0, true)).touching);
    CHECK(!run(block, sample(0, 0, 0, false)).touching);
    CHECK(run(block, sample(0, 0, 0, true)).touching);
}

NNH_TEST(the_game_and_transition_blockers_are_independent) {
    InputBlock block;
    block.set(InputBlocker::Game, true);
    block.set(InputBlocker::Transition, true);
    block.set(InputBlocker::Game, false);
    CHECK(block.blocking());
    CHECK(idle(run(block, sample(kA))));
    block.set(InputBlocker::Transition, false);
    CHECK(!block.blocking());
    block.set(InputBlocker::Transition, true);
    block.set(InputBlocker::Transition, true); // twice: still one blocker
    block.set(InputBlocker::Transition, false);
    CHECK(!block.blocking());
}
