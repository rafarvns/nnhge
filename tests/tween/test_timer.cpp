// nnh::Timer, the plain timer value (spec 015 FR-14, FR-16): no set, no scene, no nnh::init().

#include "tween_fixture.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::FrameTime;
using nnh::Timer;

namespace {

int updates_until_fired(Timer& t, int limit) {
    for (int i = 1; i <= limit; ++i) {
        if (t.update() > 0) return i;
    }
    return -1;
}

void start_with_negative_time() {
    (void)Timer::once(Duration{-1});
}
void start_repeating_with_zero_period() {
    (void)Timer::repeating(Frames{0});
}

} // namespace

NNH_TEST(a_default_timer_is_stopped_and_never_fires) {
    Timer t;
    CHECK(!t.running());
    CHECK(!t.paused());
    CHECK(t.update() == 0);
    CHECK(t.advance(Duration::from_seconds(10)) == 0);
    CHECK(sizeof(Timer) <= 12);
}

NNH_TEST(a_one_shot_timer_fires_once_on_its_last_frame) {
    Timer t = Timer::once(Duration::from_ms(500));
    CHECK(t.running());
    CHECK(updates_until_fired(t, 100) == 30);
    CHECK(!t.running());
    CHECK(t.update() == 0);
    CHECK(t.remaining().ticks == 0);
    CHECK(t.elapsed() == FrameTime(Frames{30}));
    CHECK(t.progress() == 1.0f);
}

NNH_TEST(a_one_shot_timer_of_n_frames_fires_on_update_n) {
    Timer t = Timer::once(Frames{7});
    CHECK(updates_until_fired(t, 100) == 7);
    Timer zero = Timer::once(Frames{0});
    CHECK(zero.update() == 1);
}

NNH_TEST(a_repeating_timer_fires_every_period_without_drift) {
    Timer t = Timer::repeating(Duration::from_seconds(1));
    int fired = 0;
    for (int i = 0; i < 600; ++i) {
        fired += t.update();
    }
    CHECK(fired == 10);
    CHECK(t.elapsed().ticks == 0);
    CHECK(t.running());
}

NNH_TEST(a_repeating_timer_counts_every_period_in_one_advance_and_carries_the_rest) {
    Timer t = Timer::repeating(Frames{10});
    CHECK(t.advance(Frames{35}) == 3);
    CHECK(t.elapsed() == FrameTime(Frames{5}));
    CHECK(t.remaining() == FrameTime(Frames{5}));
    CHECK(t.progress() == 0.5f);
    CHECK(t.advance(Frames{5}) == 1);
    CHECK(t.elapsed().ticks == 0);
}

NNH_TEST(a_one_shot_timer_fires_once_even_when_advanced_far_past_its_end) {
    Timer t = Timer::once(Frames{2});
    CHECK(t.advance(Frames{50}) == 1);
    CHECK(t.elapsed() == FrameTime(Frames{2}));
}

NNH_TEST(a_paused_timer_keeps_its_time_until_resumed) {
    Timer t = Timer::once(Frames{4});
    t.update();
    t.pause();
    CHECK(t.paused() && t.running());
    for (int i = 0; i < 10; ++i) {
        CHECK(t.update() == 0);
    }
    CHECK(t.elapsed() == FrameTime(Frames{1}));
    t.resume();
    CHECK(!t.paused());
    CHECK(updates_until_fired(t, 10) == 3);
}

NNH_TEST(stop_halts_and_rewinds_and_restart_starts_over) {
    Timer t = Timer::repeating(Frames{3});
    t.update();
    t.stop();
    CHECK(!t.running());
    CHECK(t.elapsed().ticks == 0);
    CHECK(t.remaining() == FrameTime(Frames{3}));
    CHECK(t.update() == 0);
    t.restart();
    CHECK(t.running());
    CHECK(updates_until_fired(t, 10) == 3);
    Timer once = Timer::once(Frames{2});
    once.advance(Frames{5});
    once.restart();
    CHECK(once.running() && once.elapsed().ticks == 0);
    CHECK(updates_until_fired(once, 10) == 2);
}

NNH_TEST(restart_clears_a_pause) {
    Timer t = Timer::once(Frames{2});
    t.pause();
    t.restart();
    CHECK(!t.paused());
    CHECK(updates_until_fired(t, 10) == 2);
}

NNH_TEST(timer_progress_and_remaining_follow_the_time) {
    Timer t = Timer::once(Frames{4});
    CHECK(t.progress() == 0.0f);
    CHECK(t.remaining() == FrameTime(Frames{4}));
    t.update();
    CHECK(t.progress() == 0.25f);
    CHECK(t.remaining() == FrameTime(Frames{3}));
    Timer zero = Timer::once(Frames{0});
    CHECK(zero.progress() == 0.0f);
    zero.update();
    CHECK(zero.progress() == 1.0f);
}

NNH_TEST(a_timer_with_an_invalid_time_asserts_or_stays_stopped) {
    CHECK(tween_test::asserts_or_is_ignored(start_with_negative_time));
    CHECK(tween_test::asserts_or_is_ignored(start_repeating_with_zero_period));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(!Timer::once(Duration{-1}).running());
        CHECK(!Timer::repeating(Frames{0}).running());
        Timer t = Timer::once(Frames{3});
        CHECK(t.advance(Duration{-5}) == 0);
        CHECK(t.elapsed().ticks == 0);
    }
}
