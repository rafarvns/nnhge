// Delay, loops, restart and yoyo (spec 015 FR-7; T2.4): loop counts, mirrored values, carry across loops.

#include "nnh/gfx.hpp"
#include "tween_fixture.hpp"

using nnh::Ease;
using nnh::Frames;
using nnh::LoopMode;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

NNH_TEST(a_delay_holds_the_tween_before_it_starts) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 4.0f, Frames{4}, {.delay = Frames{10}});
    run(set, 10);
    CHECK(x == 0.0f);
    CHECK(set.progress(h) == 0.0f);
    run(set, 1);
    CHECK(x == 1.0f);
    run(set, 3);
    CHECK(x == 4.0f && !set.running(h));
}

NNH_TEST(restart_loops_jump_back_to_the_start_value) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 4.0f, Frames{4}, {.loops = 3});
    CHECK(set.loops_left(h) == 3);
    run(set, 4);
    CHECK(x == 0.0f); // end of loop 1 = start of loop 2
    CHECK(set.loops_left(h) == 2);
    run(set, 1);
    CHECK(x == 1.0f);
    run(set, 7);
    CHECK(x == 4.0f && !set.running(h));
}

NNH_TEST(yoyo_plays_back_with_the_mirrored_easing) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 1.0f, Frames{4}, {.ease = Ease::QuadIn, .loops = 2, .loop_mode = LoopMode::Yoyo});
    run(set, 1);
    const float forward_1 = x; // QuadIn at 1/4
    CHECK(forward_1 == nnh::ease(Ease::QuadIn, 0.25f));
    run(set, 3);
    CHECK(x == 1.0f);
    run(set, 3); // backwards, 3/4 through: the forward value at 1/4
    CHECK(x == forward_1);
    run(set, 1);
    CHECK(x == 0.0f); // an even yoyo count ends on the start value
    CHECK(set.active() == 0);
}

NNH_TEST(an_odd_yoyo_count_ends_on_the_end_value) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 1.0f, Frames{2}, {.loops = 3, .loop_mode = LoopMode::Yoyo});
    run(set, 6);
    CHECK(x == 1.0f && set.active() == 0);
}

NNH_TEST(an_infinite_tween_never_finishes) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 1.0f, Frames{3}, {.loops = nnh::kForever, .loop_mode = LoopMode::Yoyo});
    run(set, 3000);
    CHECK(set.running(h));
    CHECK(set.loops_left(h) == nnh::kForever);
    CHECK(x == 0.0f); // 1000 loops ended (the last one backwards): a forward loop starts at the start value
}

NNH_TEST(loops_capture_the_start_once) {
    TweenSet set({.tweens = 4});
    float x = 2;
    set.by(&x, 2.0f, Frames{2}, {.loops = 2});
    run(set, 2);
    CHECK(x == 2.0f); // restart: back to the captured start, not start + delta
    run(set, 2);
    CHECK(x == 4.0f);
}

NNH_TEST(leftover_time_carries_into_the_next_loop) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 3.0f, nnh::FrameTime::from_ticks(3 * 4096 - 2048), {.loops = 2});
    run(set, 3); // 3 frames = loop 1 (2.5 frames) + 0.5 frame of loop 2
    CHECK(set.loops_left(h) == 1);
    CHECK(set.progress(h) == 0.2f);
    run(set, 2); // 5 frames = 2 loops exactly
    CHECK(!set.running(h));
    CHECK(x == 3.0f);
}

NNH_TEST(several_loops_can_end_in_one_advance) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 1.0f, Frames{1}, {.loops = 5});
    set.advance(Frames{3});
    CHECK(set.loops_left(h) == 2);
    set.advance(Frames{10});
    CHECK(!set.running(h) && x == 1.0f);
}

NNH_TEST(zero_length_loops_all_end_in_one_update) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 1.0f, Frames{0}, {.loops = 4, .loop_mode = LoopMode::Yoyo});
    set.update();
    CHECK(x == 0.0f && set.active() == 0);
}
