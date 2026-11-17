// Fixed-timestep accumulator (time/fixed_step_accumulator.hpp).

#include "nnh/time.hpp"
#include "test.hpp"
#include "time/fixed_step_accumulator.hpp"

using nnh::Duration;
using nnh::detail::FixedStepAccumulator;

namespace {

int steps_after(FixedStepAccumulator& acc, Duration elapsed) {
    acc.add(elapsed);
    int steps = 0;
    while (acc.consume()) ++steps;
    return steps;
}

} // namespace

NNH_TEST(one_step_per_frame_when_frame_matches_step) {
    FixedStepAccumulator acc(60, 4);
    for (int i = 0; i < 10; ++i) CHECK(steps_after(acc, acc.step()) == 1);
}

NNH_TEST(two_steps_when_a_frame_takes_twice_as_long) {
    FixedStepAccumulator acc(60, 4);
    CHECK(steps_after(acc, acc.step() * 2) == 2);
}

NNH_TEST(leftover_time_carries_to_the_next_frame) {
    FixedStepAccumulator acc(25, 4); // 40 ms steps (exact in ns), frames of half a step
    const Duration half = Duration{acc.step().ns / 2};
    CHECK(steps_after(acc, half) == 0);
    CHECK(steps_after(acc, half) == 1);
    CHECK(steps_after(acc, half) == 0);
    CHECK(steps_after(acc, half) == 1);
}

NNH_TEST(catch_up_is_capped_after_a_long_hitch) {
    FixedStepAccumulator acc(60, 4);
    CHECK(steps_after(acc, Duration::from_seconds(5)) == 4);
    CHECK(steps_after(acc, acc.step()) == 1); // the excess was dropped, not kept for later
}

NNH_TEST(alpha_is_the_fraction_of_a_step_left_over) {
    FixedStepAccumulator acc(10, 4); // 100 ms steps
    steps_after(acc, Duration::from_ms(125));
    CHECK(acc.alpha() > 0.24f && acc.alpha() < 0.26f);
}
