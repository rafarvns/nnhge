// Timers in a set (spec 015 FR-13, FR-16; T3.2): `after` with a callback or an event, `every` n times or
// forever, several fires per advance, the minimum period.

#include "nnh/events.hpp"
#include "tween_fixture.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;

namespace {

struct ReadyGo {
    int round;
};

/// Updates until `*count` changes; returns the update number (or -1 past `limit`).
int updates_until_called(TweenSet& set, const int* count, int limit = 1000) {
    const int before = *count;
    for (int i = 1; i <= limit; ++i) {
        set.update();
        if (*count != before) return i;
    }
    return -1;
}

TweenSet* g_set = nullptr;
void every_with_zero_period() {
    (void)g_set->every(Frames{0}, [] {});
}
void every_with_zero_times() {
    (void)g_set->every(Frames{1}, [] {}, 0);
}
void after_with_negative_time() {
    (void)g_set->after(Duration{-1}, [] {});
}

} // namespace

NNH_TEST(after_calls_back_once_when_the_time_has_passed) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    const TweenHandle h = set.after(Duration::from_seconds(1), [&calls] { ++calls; });
    CHECK(set.running(h));
    CHECK(updates_until_called(set, &calls) == 60);
    CHECK(!set.running(h));
    tween_test::run(set, 100);
    CHECK(calls == 1);
}

NNH_TEST(after_takes_whole_frames_and_zero) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    set.after(Frames{5}, [&calls] { ++calls; });
    CHECK(updates_until_called(set, &calls) == 5);
    set.after(Frames{0}, [&calls] { ++calls; });
    CHECK(updates_until_called(set, &calls) == 1);
}

NNH_TEST(after_can_publish_a_game_event) {
    tween_test::restart();
    {
        TweenSet set({.tweens = 4});
        int received = 0, round = 0;
        nnh::Subscription sub = nnh::events::subscribe<ReadyGo>([&](const ReadyGo& e) {
            ++received;
            round = e.round;
        });
        set.after(Frames{3}, ReadyGo{2});
        tween_test::run(set, 3);
        CHECK(nnh::frame());
        CHECK(received == 1 && round == 2);
    }
    nnh::shutdown();
}

NNH_TEST(every_calls_back_each_period_n_times) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    const TweenHandle h = set.every(Duration::from_ms(500), [&calls] { ++calls; }, 3);
    CHECK(updates_until_called(set, &calls) == 30);
    CHECK(updates_until_called(set, &calls) == 30);
    CHECK(set.running(h) && set.loops_left(h) == 1);
    CHECK(updates_until_called(set, &calls) == 30);
    CHECK(!set.running(h));
    tween_test::run(set, 100);
    CHECK(calls == 3);
}

NNH_TEST(every_without_a_count_runs_forever_without_drift) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    const TweenHandle h = set.every(Duration::from_seconds(2), [&calls] { ++calls; });
    tween_test::run(set, 600);
    CHECK(calls == 5);
    CHECK(set.running(h) && set.loops_left(h) == nnh::kForever);
}

NNH_TEST(every_fires_for_each_period_crossed_in_one_advance) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    set.every(Frames{2}, [&calls] { ++calls; });
    set.advance(Frames{9});
    CHECK(calls == 4);
    set.advance(Frames{1});
    CHECK(calls == 5);
    int fast = 0;
    set.every(nnh::FrameTime::from_ticks(1024), [&fast] { ++fast; }); // four per frame
    set.update();
    CHECK(fast == 4);
}

NNH_TEST(timers_answer_queries_and_commands_like_tweens) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    const TweenHandle h = set.after(Frames{4}, [&calls] { ++calls; });
    tween_test::run(set, 1);
    CHECK(set.progress(h) == 0.25f);
    set.pause(h);
    tween_test::run(set, 10);
    CHECK(calls == 0 && set.paused(h));
    set.complete(h);
    CHECK(calls == 1 && !set.running(h));
    const TweenHandle tick = set.every(Frames{1}, [&calls] { ++calls; });
    set.kill(tick);
    tween_test::run(set, 5);
    CHECK(calls == 1);
}

NNH_TEST(completing_a_repeating_timer_calls_back_once) {
    TweenSet set({.tweens = 4});
    int calls = 0;
    const TweenHandle h = set.every(Frames{3}, [&calls] { ++calls; }, 10);
    set.complete(h);
    CHECK(calls == 1 && !set.running(h));
}

NNH_TEST(invalid_timers_assert_or_are_ignored) {
    TweenSet set({.tweens = 4});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(every_with_zero_period));
    CHECK(tween_test::asserts_or_is_ignored(every_with_zero_times));
    CHECK(tween_test::asserts_or_is_ignored(after_with_negative_time));
    if (!nnh::build::kAssertsEnabled) CHECK(set.active() == 0);
}
