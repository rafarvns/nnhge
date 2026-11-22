// Sequences (spec 015 FR-10; T4.1, T4.2): building (steps, join, wait, call, limits, failed builds, the first
// group's `from`) and playing (groups one after another, the longest member, carry, captures when reached,
// zero-length groups, children's completion).

#include "nnh/events.hpp"
#include "tween_fixture.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

namespace {

struct Values {
    float x = 0, y = 0, z = 0;
    int calls = 0;
    char order[16] = {};
    int n = 0;
    void mark(char c) {
        if (n < 15) order[n++] = c;
    }
};

TweenSet* g_set = nullptr;
float g_value = 0;

void too_many_steps() {
    nnh::SequenceBuilder b = g_set->sequence();
    for (int i = 0; i <= nnh::kMaxSequenceSteps; ++i) b.wait(Frames{1});
}
void out_of_step_records() {
    (void)g_set->sequence().wait(Frames{1}).wait(Frames{1}).wait(Frames{1});
}
void infinite_child() {
    (void)g_set->sequence().to(&g_value, 1.0f, Frames{2}, {.loops = nnh::kForever});
}

} // namespace

NNH_TEST(steps_play_one_after_another) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    const TweenHandle h = set.sequence().to(&v.x, 10.0f, Frames{10}).to(&v.y, 5.0f, Frames{5});
    CHECK(set.running(h));
    run(set, 4);
    CHECK(v.x == 4.0f && v.y == 0.0f);
    run(set, 6);
    CHECK(v.x == 10.0f && v.y == 0.0f);
    run(set, 2);
    CHECK(v.y == 2.0f);
    run(set, 3);
    CHECK(v.y == 5.0f && !set.running(h));
}

NNH_TEST(a_step_captures_its_start_when_the_sequence_reaches_it) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence().to(&v.x, 10.0f, Frames{2}).to(&v.y, 10.0f, Frames{2});
    run(set, 1);
    v.y = 6; // before the second step starts
    run(set, 2);
    CHECK(v.y == 8.0f);
}

NNH_TEST(chained_steps_on_one_target_start_where_the_previous_ended) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence().to(&v.x, 10.0f, Frames{2}).to(&v.x, 0.0f, Frames{2});
    run(set, 3);
    CHECK(v.x == 5.0f);
}

NNH_TEST(join_plays_steps_together_and_the_group_lasts_as_its_longest_member) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence().to(&v.x, 10.0f, Frames{10}).join().to(&v.y, 4.0f, Frames{4}).to(&v.z, 2.0f, Frames{2});
    run(set, 2);
    CHECK(v.x == 2.0f && v.y == 2.0f);
    run(set, 2);
    CHECK(v.y == 4.0f && v.z == 0.0f);
    run(set, 7); // the group ended at 10 (x's length): z is 1 frame in
    CHECK(v.x == 10.0f && v.z == 1.0f); // z: 0 → 2 over 2 frames
}

NNH_TEST(a_member_with_loops_and_delay_counts_its_whole_length) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence()
        .to(&v.x, 1.0f, Frames{2}, {.delay = Frames{1}, .loops = 3})
        .join()
        .to(&v.y, 1.0f, Frames{1})
        .to(&v.z, 1.0f, Frames{1});
    run(set, 7);
    CHECK(v.z == 0.0f); // group = 1 + 2 × 3 frames
    run(set, 1);
    CHECK(v.z == 1.0f);
}

NNH_TEST(wait_inserts_a_pause) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence().to(&v.x, 1.0f, Frames{1}).wait(Frames{5}).to(&v.y, 1.0f, Frames{1});
    run(set, 6);
    CHECK(v.x == 1.0f && v.y == 0.0f);
    run(set, 1);
    CHECK(v.y == 1.0f);
}

NNH_TEST(call_steps_run_when_reached_and_zero_length_groups_share_the_update) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence()
        .call([p = &v] { p->mark('a'); })
        .call([p = &v] { p->mark('b'); })
        .to(&v.x, 1.0f, Frames{2})
        .call([p = &v] { p->mark('c'); })
        .to(&v.y, 1.0f, Frames{1});
    set.update();
    CHECK(v.n == 2 && v.order[0] == 'a' && v.order[1] == 'b');
    set.update(); // x ends; c runs in the same update; y starts counting
    CHECK(v.n == 3 && v.order[2] == 'c' && v.x == 1.0f);
    set.update();
    CHECK(v.y == 1.0f);
}

NNH_TEST(three_half_second_steps_end_on_update_90) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    const TweenHandle h = set.sequence()
                              .to(&v.x, 1.0f, Duration::from_ms(500))
                              .to(&v.y, 1.0f, Duration::from_ms(500))
                              .to(&v.z, 1.0f, Duration::from_ms(500));
    run(set, 89);
    CHECK(set.running(h) && v.z < 1.0f);
    run(set, 1);
    CHECK(!set.running(h) && v.z == 1.0f);
}

NNH_TEST(leftover_time_carries_into_the_next_step) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    const nnh::FrameTime half = nnh::FrameTime::from_ticks(2048);
    set.sequence().to(&v.x, 1.0f, half).to(&v.y, 4.0f, Frames{2});
    set.update(); // x ends at half a frame; y gets the other half
    CHECK(v.x == 1.0f && v.y == 1.0f);
}

NNH_TEST(a_sequence_delay_holds_every_step) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    set.sequence({.delay = Frames{3}}).from_to(&v.x, 5.0f, 6.0f, Frames{1});
    CHECK(v.x == 0.0f); // a delayed sequence writes nothing at creation
    run(set, 3);
    CHECK(v.x == 5.0f);
    run(set, 1);
    CHECK(v.x == 6.0f);
}

NNH_TEST(the_first_group_writes_its_from_values_when_appended) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    v.y = 9;
    set.sequence().from_to(&v.x, 3.0f, 4.0f, Frames{2}).join().from_to(&v.z, 7.0f, 8.0f, Frames{2}).from_to(
        &v.y, 1.0f, 2.0f, Frames{2});
    CHECK(v.x == 3.0f && v.z == 7.0f);
    CHECK(v.y == 9.0f); // second group: written when reached
}

NNH_TEST(a_childs_on_finish_and_event_fire_when_that_child_ends) {
    struct StepDone {
        int step;
    };
    tween_test::restart();
    {
        TweenSet set({.tweens = 8, .steps = 8});
        Values v;
        int received = 0;
        nnh::Subscription sub = nnh::events::subscribe<StepDone>([&](const StepDone& e) { received += e.step; });
        set.sequence()
            .to(&v.x, 1.0f, Frames{2}, {.on_finish = [p = &v] { p->mark('x'); }, .publish = StepDone{1}})
            .to(&v.y, 1.0f, Frames{2}, {.on_finish = [p = &v] { p->mark('y'); }});
        run(set, 2);
        CHECK(v.n == 1 && v.order[0] == 'x');
        CHECK(nnh::frame());
        CHECK(received == 1);
        run(set, 2);
        CHECK(v.n == 2 && v.order[1] == 'y');
    }
    nnh::shutdown();
}

NNH_TEST(a_sequence_with_only_calls_finishes_on_its_first_update) {
    TweenSet set({.tweens = 8, .steps = 8});
    Values v;
    const TweenHandle h = set.sequence({.on_finish = [p = &v] { p->mark('f'); }}).call([p = &v] { p->mark('c'); });
    set.update();
    CHECK(v.n == 2 && v.order[0] == 'c' && v.order[1] == 'f' && !set.running(h));
}

NNH_TEST(each_step_uses_one_slot_and_one_step_record) {
    TweenSet set({.tweens = 4, .steps = 2});
    Values v;
    const TweenHandle h = set.sequence().to(&v.x, 1.0f, Frames{1}).wait(Frames{1});
    CHECK(set.running(h));
    CHECK(set.to(&v.y, 1.0f, Frames{1}) != TweenHandle{}); // the 4th slot
    set.update();
    set.update(); // the sequence ends: its 3 slots are free again
    CHECK(set.after(Frames{1}, [] {}) != TweenHandle{});
    CHECK(set.after(Frames{1}, [] {}) != TweenHandle{});
    CHECK(set.after(Frames{1}, [] {}) != TweenHandle{});
}

NNH_TEST(a_failed_build_removes_the_partial_sequence) {
    TweenSet set({.tweens = 80, .steps = 2});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(out_of_step_records));
    CHECK(tween_test::asserts_or_is_ignored(infinite_child));
    if (!nnh::build::kAssertsEnabled) {
        const TweenHandle h = set.sequence().wait(Frames{1}).wait(Frames{1}).wait(Frames{1});
        CHECK(h == TweenHandle{});
        CHECK(set.active() == 0);
        const TweenHandle ok = set.sequence().wait(Frames{1}).wait(Frames{1}); // everything was freed
        CHECK(set.running(ok));
    }
    TweenSet big({.tweens = 80, .steps = 80});
    g_set = &big;
    CHECK(tween_test::asserts_or_is_ignored(too_many_steps));
    if (!nnh::build::kAssertsEnabled) CHECK(big.active() == 0);
}
