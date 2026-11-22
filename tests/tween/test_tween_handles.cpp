// Handles (spec 015 FR-8; T2.5): queries and commands, stale handles after finish/kill/reuse, the empty
// handle, handles of another set.

#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::LoopMode;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

namespace {

TweenSet* g_set = nullptr;
TweenHandle g_foreign{};
void query_with_foreign_handle() {
    (void)g_set->running(g_foreign);
}
void kill_with_foreign_handle() {
    g_set->kill(g_foreign);
}

} // namespace

NNH_TEST(queries_follow_a_playing_tween) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 1.0f, Frames{4}, {.loops = 2});
    CHECK(set.running(h) && !set.paused(h));
    CHECK(set.progress(h) == 0.0f);
    CHECK(set.loops_left(h) == 2);
    run(set, 1);
    CHECK(set.progress(h) == 0.25f);
    run(set, 4);
    CHECK(set.progress(h) == 0.25f);
    CHECK(set.loops_left(h) == 1);
    CHECK(set.active() == 1);
}

NNH_TEST(pause_freezes_one_tween_and_resume_continues_it) {
    TweenSet set({.tweens = 4});
    float x = 0, y = 0;
    const TweenHandle hx = set.to(&x, 4.0f, Frames{4});
    set.to(&y, 4.0f, Frames{4});
    run(set, 1);
    set.pause(hx);
    CHECK(set.paused(hx) && set.running(hx));
    run(set, 2);
    CHECK(x == 1.0f && y == 3.0f);
    set.resume(hx);
    CHECK(!set.paused(hx));
    run(set, 1);
    CHECK(x == 2.0f);
}

NNH_TEST(kill_stops_a_tween_where_it_is) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 4.0f, Frames{4});
    run(set, 2);
    set.kill(h);
    CHECK(!set.running(h));
    CHECK(set.active() == 0);
    run(set, 5);
    CHECK(x == 2.0f);
}

NNH_TEST(complete_jumps_to_the_end_value) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 4.0f, Frames{40});
    run(set, 3);
    set.complete(h);
    CHECK(x == 4.0f && !set.running(h));
}

NNH_TEST(complete_during_the_delay_captures_the_start_first) {
    TweenSet set({.tweens = 4});
    float x = 3;
    set.complete(set.by(&x, 2.0f, Frames{4}, {.delay = Frames{10}}));
    CHECK(x == 5.0f);
}

NNH_TEST(complete_ends_on_the_value_of_the_last_loop) {
    TweenSet set({.tweens = 4});
    float even = 0, odd = 0, forever = 0;
    const TweenHandle he = set.to(&even, 1.0f, Frames{4}, {.loops = 4, .loop_mode = LoopMode::Yoyo});
    const TweenHandle ho = set.to(&odd, 1.0f, Frames{4}, {.loops = 3, .loop_mode = LoopMode::Yoyo});
    const TweenHandle hf =
        set.to(&forever, 1.0f, Frames{4}, {.loops = nnh::kForever, .loop_mode = LoopMode::Yoyo});
    run(set, 5); // all in their second (backward) loop
    set.complete(he);
    set.complete(ho);
    set.complete(hf);
    CHECK(even == 0.0f);
    CHECK(odd == 1.0f);
    CHECK(forever == 0.0f); // infinite: the end of the current (backward) loop
    CHECK(set.active() == 0);
}

NNH_TEST(handles_go_stale_after_finish_kill_and_slot_reuse) {
    TweenSet set({.tweens = 1});
    float x = 0;
    const TweenHandle first = set.to(&x, 1.0f, Frames{1});
    run(set, 1);
    CHECK(!set.running(first));
    const TweenHandle second = set.to(&x, 2.0f, Frames{10}); // same slot, new generation
    CHECK(second.slot == first.slot && second.generation != first.generation);
    set.kill(first); // stale: no effect on the new tween
    set.pause(first);
    set.complete(first);
    CHECK(set.running(second) && !set.paused(second));
    CHECK(set.progress(first) == 0.0f && set.loops_left(first) == 0);
    set.kill(second);
    CHECK(!set.running(second));
}

NNH_TEST(the_empty_handle_is_stale_and_harmless) {
    TweenSet set({.tweens = 2});
    const TweenHandle empty{};
    CHECK(!set.running(empty) && !set.paused(empty));
    set.kill(empty);
    set.complete(empty);
    set.pause(empty);
    set.resume(empty);
    CHECK(set.progress(empty) == 0.0f);
    TweenSet no_storage;
    CHECK(!no_storage.running(empty));
}

NNH_TEST(a_handle_of_another_set_asserts_or_is_ignored) {
    TweenSet a({.tweens = 2}), b({.tweens = 2});
    float x = 0;
    g_foreign = a.to(&x, 1.0f, Frames{10});
    g_set = &b;
    CHECK(tween_test::asserts_or_is_ignored(query_with_foreign_handle));
    CHECK(tween_test::asserts_or_is_ignored(kill_with_foreign_handle));
    CHECK(a.running(g_foreign));
}
