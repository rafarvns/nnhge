// Group commands (spec 015 FR-9; T2.5): pause/resume/kill all, kill_target, two tweens on one target.

#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;
using nnh::Vec2;
using tween_test::run;

NNH_TEST(pause_all_and_resume_all_affect_every_tween) {
    TweenSet set({.tweens = 4});
    float x = 0, y = 0;
    const TweenHandle hx = set.to(&x, 4.0f, Frames{4});
    const TweenHandle hy = set.to(&y, 4.0f, Frames{4}, {.delay = Frames{1}});
    run(set, 1);
    set.pause_all();
    CHECK(set.paused(hx) && set.paused(hy));
    run(set, 3);
    CHECK(x == 1.0f && y == 0.0f);
    set.resume_all();
    run(set, 1);
    CHECK(x == 2.0f && y == 1.0f);
}

NNH_TEST(kill_all_stops_everything_silently) {
    TweenSet set({.tweens = 4});
    float x = 0, y = 0;
    set.to(&x, 4.0f, Frames{4});
    set.to(&y, 4.0f, Frames{4});
    run(set, 1);
    set.kill_all();
    CHECK(set.active() == 0);
    run(set, 5);
    CHECK(x == 1.0f && y == 1.0f);
    float z = 0;
    set.to(&z, 1.0f, Frames{1}); // the slots are free again
    set.to(&z, 1.0f, Frames{1});
    set.to(&z, 1.0f, Frames{1});
    set.to(&z, 1.0f, Frames{1});
    CHECK(set.active() == 4);
}

NNH_TEST(kill_target_kills_only_the_tweens_on_that_address) {
    TweenSet set({.tweens = 4});
    Vec2 enemy{0, 0};
    float alpha = 0, other = 0;
    const TweenHandle move = set.to(&enemy, {10, 10}, Frames{10});
    const TweenHandle fade = set.to(&alpha, 1.0f, Frames{10});
    const TweenHandle keep = set.to(&other, 1.0f, Frames{10});
    const TweenHandle move2 = set.by(&enemy, {1, 0}, Frames{10});
    run(set, 1);
    set.kill_target(&enemy);
    CHECK(!set.running(move) && !set.running(move2));
    CHECK(set.running(fade) && set.running(keep));
    set.kill_target(&alpha);
    CHECK(!set.running(fade) && set.running(keep));
    set.kill_target(nullptr); // no pointer target is null: nothing happens
    CHECK(set.running(keep));
}

NNH_TEST(kill_target_of_null_spares_timers_and_setters) {
    TweenSet set({.tweens = 4});
    float sink = 0;
    const TweenHandle timer = set.after(Frames{10}, [] {});
    const TweenHandle setter =
        set.from_to(nnh::Setter<float>{[&sink](float v) { sink = v; }}, 0.0f, 1.0f, Frames{10});
    set.kill_target(nullptr);
    CHECK(set.running(timer) && set.running(setter));
}

NNH_TEST(two_tweens_on_one_target_both_write_and_the_later_wins) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle a = set.from_to(&x, 0.0f, 10.0f, Frames{10});
    const TweenHandle b = set.from_to(&x, 100.0f, 200.0f, Frames{5});
    CHECK(x == 100.0f);
    run(set, 1);
    CHECK(x == 120.0f);
    run(set, 4);
    CHECK(x == 200.0f && !set.running(b) && set.running(a));
    run(set, 1);
    CHECK(x == 6.0f); // only the first one is left
}
