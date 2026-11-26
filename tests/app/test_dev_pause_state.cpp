// The pause state machine (spec 030 T3.2, FR-16, D52): every transition, a step while running pauses first, step N
// runs exactly N frames, the held start with and without a PC, its timeout.

#include "app/dev_pause.hpp"
#include "test.hpp"

using nnh::detail::PauseState;

namespace {

/// Frame starts until `limit`: how many ran a game frame before the first paused tick.
int frames_run(PauseState& p, int limit) {
    int n = 0;
    while (n < limit && p.begin_frame()) ++n;
    return n;
}

} // namespace

NNH_TEST(dev_pause_runs_until_asked) {
    PauseState p;
    CHECK(!p.paused());
    CHECK(frames_run(p, 100) == 100);
}

NNH_TEST(dev_pause_pause_takes_effect_at_the_next_frame_start) {
    PauseState p;
    p.pause();
    CHECK(!p.begin_frame());
    CHECK(p.paused());
    CHECK(!p.tick(false));
    CHECK(!p.begin_frame());
}

NNH_TEST(dev_pause_resume_runs_again) {
    PauseState p;
    p.pause();
    CHECK(!p.begin_frame());
    p.resume();
    CHECK(p.begin_frame());
    CHECK(!p.paused());
}

NNH_TEST(dev_pause_toggle_flips) {
    PauseState p;
    p.toggle();
    CHECK(!p.begin_frame());
    p.toggle();
    CHECK(p.begin_frame());
}

NNH_TEST(dev_pause_step_n_runs_exactly_n_frames) {
    PauseState p;
    p.pause();
    CHECK(!p.begin_frame());
    p.step(3);
    CHECK(p.steps_left() == 3);
    CHECK(frames_run(p, 100) == 3);
    CHECK(p.paused() && p.steps_left() == 0);
    p.step(1);
    CHECK(frames_run(p, 100) == 1);
}

NNH_TEST(dev_pause_step_while_running_pauses_first) {
    PauseState p;
    p.step(2);
    CHECK(frames_run(p, 100) == 2);
    CHECK(p.paused());
}

NNH_TEST(dev_pause_resume_while_stepping_runs_on) {
    PauseState p;
    p.pause();
    p.step(5);
    CHECK(p.begin_frame());
    p.resume();
    CHECK(frames_run(p, 50) == 50);
}

NNH_TEST(dev_pause_pause_while_stepping_stops_the_steps) {
    PauseState p;
    p.step(5);
    CHECK(p.begin_frame());
    p.pause();
    CHECK(!p.begin_frame());
    CHECK(p.steps_left() == 0);
}

NNH_TEST(dev_pause_held_start_waits_for_a_resume) {
    PauseState p;
    p.hold();
    CHECK(p.held() && p.paused());
    for (int i = 0; i < 1000; ++i) {
        CHECK(!p.begin_frame());
        CHECK(!p.tick(true)); // a PC is attached: no timeout
    }
    p.resume();
    CHECK(p.begin_frame());
    CHECK(!p.held());
}

NNH_TEST(dev_pause_held_start_times_out_without_a_pc) {
    PauseState p;
    p.hold();
    int ticks = 0;
    bool timed_out = false;
    while (!p.begin_frame()) {
        ++ticks;
        if (p.tick(false)) timed_out = true;
        if (ticks > 1000) break;
    }
    CHECK(timed_out);
    CHECK(ticks == PauseState::kHoldTicks);
    CHECK(!p.held() && !p.paused());
}

NNH_TEST(dev_pause_held_start_ends_with_a_step) {
    PauseState p;
    p.hold();
    CHECK(!p.begin_frame());
    p.step(1);
    CHECK(frames_run(p, 10) == 1);
    CHECK(!p.held() && p.paused());
}
