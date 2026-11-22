// Whole-sequence loops, yoyo, completion and commands (spec 015 FR-9..FR-12; T4.3): restart, yoyo (steps
// backwards in reverse order, calls in both directions), sequence callbacks and events, complete, kill,
// kill_target, pause, and callbacks inside call steps.

#include "nnh/events.hpp"
#include "tween_fixture.hpp"

using nnh::Ease;
using nnh::Frames;
using nnh::LoopMode;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

namespace {

struct Ctx {
    TweenSet set{{.tweens = 16, .steps = 16}};
    TweenHandle seq{};
    float x = 0, y = 0, z = 0;
    int calls = 0, loops = 0, finishes = 0, child_finishes = 0;
};

TweenSet* g_set = nullptr;
void forever_with_zero_length() {
    (void)g_set->sequence({.loops = nnh::kForever}).call([] {});
    g_set->update();
}

} // namespace

NNH_TEST(restart_loops_replay_every_step_from_its_captured_start) {
    Ctx c;
    c.seq =
        c.set
            .sequence(
                {.loops = 3, .on_finish = [p = &c] { ++p->finishes; }, .on_loop = [p = &c] { ++p->loops; }})
            .to(&c.x, 2.0f, Frames{2})
            .call([p = &c] { ++p->calls; });
    run(c.set, 1);
    CHECK(c.x == 1.0f);
    run(c.set, 1); // loop 1 ends (x = 2, call), loop 2 starts in the same update (x back to its start)
    CHECK(c.calls == 1 && c.loops == 1 && c.x == 0.0f);
    run(c.set, 1);
    CHECK(c.x == 1.0f);
    run(c.set, 3);
    CHECK(c.calls == 3 && c.loops == 2 && c.finishes == 1 && c.x == 2.0f);
    CHECK(!c.set.running(c.seq));
}

NNH_TEST(a_restart_replays_a_call_at_the_start_of_the_loop) {
    Ctx c;
    c.set.sequence({.loops = 3}).call([p = &c] { ++p->calls; }).to(&c.x, 1.0f, Frames{2});
    run(c.set, 1);
    CHECK(c.calls == 1);
    run(c.set, 1); // loop 2 starts in this update: its call runs here
    CHECK(c.calls == 2);
    run(c.set, 4);
    CHECK(c.calls == 3);
}

NNH_TEST(a_looping_yoyo_step_inside_a_sequence_mirrors_and_ends_on_its_start) {
    Ctx c;
    c.set.sequence()
        .to(&c.x, 4.0f, Frames{4}, {.ease = Ease::QuadIn, .loops = 2, .loop_mode = LoopMode::Yoyo})
        .to(&c.y, 1.0f, Frames{1});
    run(c.set, 1); // the step's first loop plays forwards
    CHECK(c.x == 4.0f * nnh::ease(Ease::QuadIn, 0.25f));
    run(c.set, 4); // the step's second loop, backwards, 1/4 in: its forward value at 3/4
    CHECK(c.x == 4.0f * nnh::ease(Ease::QuadIn, 0.75f));
    run(c.set, 3);
    CHECK(c.x == 0.0f && c.y == 0.0f); // the step ended on its start value; y starts now
}

NNH_TEST(a_looping_step_calls_on_loop_between_its_loops) {
    Ctx c;
    c.set.sequence().to(&c.x, 1.0f, Frames{2},
                        {.delay = Frames{1}, .loops = 3, .on_loop = [p = &c] { ++p->loops; }});
    run(c.set, 2);
    CHECK(c.loops == 0);
    run(c.set, 1);
    CHECK(c.loops == 1);
    run(c.set, 10);
    CHECK(c.loops == 2);
}

NNH_TEST(a_step_back_at_its_start_is_not_written_again) {
    Ctx c;
    int writes = 0;
    c.set.sequence({.loops = 2, .loop_mode = LoopMode::Yoyo})
        .from_to(nnh::Setter<float>{[&writes](float) { ++writes; }}, 0.0f, 1.0f, Frames{2},
                 {.delay = Frames{2}});
    run(c.set, 4); // forward: written when its delay ends (frame 2), at 3, at its end (4)
    CHECK(writes == 3);
    run(c.set,
        4); // backward: at 3, back at its start at 2; then nothing while the sequence crosses its delay
    CHECK(writes == 5);
}

NNH_TEST(a_child_callback_that_pauses_its_sequence_stops_further_loops) {
    Ctx c;
    c.seq = c.set.sequence({.loops = 10}).to(&c.x, 1.0f, Frames{1}, {.on_finish = [p = &c] {
        ++p->child_finishes;
        p->set.pause(p->seq);
    }});
    c.set.advance(Frames{5});
    // Paused at the end of its first loop: that loop end waits for the resume.
    CHECK(c.child_finishes == 1 && c.set.paused(c.seq) && c.set.loops_left(c.seq) == 10);
    c.set.resume(c.seq);
    c.set.update();
    CHECK(c.set.loops_left(c.seq) == 9 && c.child_finishes == 2); // the next loop ended it and paused again
}

NNH_TEST(yoyo_plays_the_steps_backwards_in_reverse_order) {
    Ctx c;
    c.seq = c.set.sequence({.loops = 2, .loop_mode = LoopMode::Yoyo})
                .to(&c.x, 4.0f, Frames{4}, {.ease = Ease::QuadIn})
                .to(&c.y, 4.0f, Frames{4});
    run(c.set, 8);
    CHECK(c.x == 4.0f && c.y == 4.0f);
    run(c.set, 1);
    CHECK(c.y == 3.0f && c.x == 4.0f);
    run(c.set, 3);
    CHECK(c.y == 0.0f && c.x == 4.0f);
    run(c.set, 1); // x backwards, 3/4 of the way: the forward value at 3/4 (mirrored easing)
    CHECK(c.x == 4.0f * nnh::ease(Ease::QuadIn, 0.75f));
    run(c.set, 3);
    CHECK(c.x == 0.0f && !c.set.running(c.seq)); // an even yoyo count ends on the start values
}

NNH_TEST(call_steps_run_in_both_directions_but_not_twice_at_a_turn) {
    Ctx c;
    c.set.sequence({.loops = 2, .loop_mode = LoopMode::Yoyo})
        .to(&c.x, 1.0f, Frames{2})
        .call([p = &c] { ++p->calls; })
        .to(&c.y, 1.0f, Frames{2});
    run(c.set, 2);
    CHECK(c.calls == 1);
    run(c.set, 4); // back past the call
    CHECK(c.calls == 2);
    Ctx d;
    d.set.sequence({.loops = 3, .loop_mode = LoopMode::Yoyo}).to(&d.x, 1.0f, Frames{2}).call([p = &d] {
        ++p->calls;
    });
    run(d.set, 4); // forward to the call at the end, then straight back
    CHECK(d.calls == 1);
    run(d.set, 2); // forward again: passes it once more
    CHECK(d.calls == 2);
}

NNH_TEST(a_sequence_publishes_its_event_when_it_finishes) {
    struct IntroFinished {};
    tween_test::restart();
    {
        Ctx c;
        int received = 0;
        nnh::Subscription sub =
            nnh::events::subscribe<IntroFinished>([&](const IntroFinished&) { ++received; });
        c.set.sequence({.publish = IntroFinished{}}).to(&c.x, 1.0f, Frames{2}).wait(Frames{1});
        run(c.set, 2);
        CHECK(nnh::frame());
        CHECK(received == 0);
        run(c.set, 1);
        CHECK(nnh::frame());
        CHECK(received == 1);
    }
    nnh::shutdown();
}

NNH_TEST(complete_jumps_every_step_to_its_end_and_runs_only_the_sequence_completion) {
    Ctx c;
    c.y = 7;
    c.seq = c.set.sequence({.on_finish = [p = &c] { ++p->finishes; }})
                .to(&c.x, 1.0f, Frames{4}, {.on_finish = [p = &c] { ++p->child_finishes; }})
                .call([p = &c] { ++p->calls; })
                .by(&c.y, 1.0f, Frames{4});
    run(c.set, 1);
    c.set.complete(c.seq);
    CHECK(c.x == 1.0f && c.y == 8.0f); // y was never reached: captured first
    CHECK(c.finishes == 1 && c.child_finishes == 0 && c.calls == 0);
    CHECK(!c.set.running(c.seq) && c.set.active() == 0);
}

NNH_TEST(completing_an_even_yoyo_sequence_ends_on_the_start_values) {
    Ctx c;
    c.x = 3;
    c.seq = c.set.sequence({.loops = 2, .loop_mode = LoopMode::Yoyo}).to(&c.x, 9.0f, Frames{4});
    run(c.set, 1);
    c.set.complete(c.seq);
    CHECK(c.x == 3.0f);
}

NNH_TEST(kill_stops_a_sequence_silently_and_frees_its_slots) {
    Ctx c;
    c.seq = c.set.sequence({.on_finish = [p = &c] { ++p->finishes; }})
                .to(&c.x, 4.0f, Frames{4}, {.on_finish = [p = &c] { ++p->child_finishes; }})
                .to(&c.y, 4.0f, Frames{4});
    run(c.set, 2);
    c.set.kill(c.seq);
    run(c.set, 10);
    CHECK(c.x == 2.0f && c.y == 0.0f && c.finishes == 0 && c.child_finishes == 0);
    for (int i = 0; i < 16; ++i) {
        CHECK(c.set.after(Frames{1}, [] {}) != TweenHandle{});
    }
}

NNH_TEST(kill_target_kills_the_whole_sequence_that_animates_it) {
    Ctx c;
    c.seq = c.set.sequence().to(&c.x, 4.0f, Frames{4}).to(&c.y, 4.0f, Frames{4});
    const TweenHandle other = c.set.to(&c.z, 1.0f, Frames{10});
    run(c.set, 1);
    c.set.kill_target(&c.y);
    CHECK(!c.set.running(c.seq) && c.set.running(other));
    run(c.set, 10);
    CHECK(c.x == 1.0f && c.y == 0.0f);
}

NNH_TEST(a_paused_sequence_holds_and_reports_its_progress) {
    Ctx c;
    c.seq = c.set.sequence().to(&c.x, 4.0f, Frames{4}).to(&c.y, 4.0f, Frames{4});
    run(c.set, 2);
    CHECK(c.set.progress(c.seq) == 0.25f);
    c.set.pause_all();
    run(c.set, 5);
    CHECK(c.set.paused(c.seq) && c.x == 2.0f);
    c.set.resume(c.seq);
    run(c.set, 6);
    CHECK(c.y == 4.0f && !c.set.running(c.seq));
}

NNH_TEST(a_call_step_can_kill_its_own_sequence) {
    Ctx c;
    c.seq = c.set.sequence({.on_finish = [p = &c] { ++p->finishes; }})
                .to(&c.x, 1.0f, Frames{1})
                .call([p = &c] { p->set.kill(p->seq); })
                .from_to(&c.y, 5.0f, 6.0f, Frames{1}); // would write 5 if it were reached
    run(c.set, 5);
    CHECK(c.x == 1.0f && c.y == 0.0f && c.finishes == 0);
    CHECK(c.set.active() == 0);
}

NNH_TEST(a_call_step_can_start_tweens_and_complete_its_sequence) {
    Ctx c;
    c.seq = c.set.sequence({.on_finish = [p = &c] { ++p->finishes; }})
                .call([p = &c] {
                    p->set.to(&p->z, 1.0f, Frames{1});
                    p->set.complete(p->seq);
                })
                .to(&c.x, 1.0f, Frames{10});
    c.set.update();
    CHECK(c.finishes == 1 && c.x == 1.0f && !c.set.running(c.seq));
    CHECK(c.z == 0.0f); // started inside the update: counts from the next one
    c.set.update();
    CHECK(c.z == 1.0f);
}

NNH_TEST(an_infinite_sequence_of_length_0_asserts_or_is_removed) {
    TweenSet set({.tweens = 4, .steps = 4});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(forever_with_zero_length));
    tween_test::recover_after_crash(set);
    if (!nnh::build::kAssertsEnabled) CHECK(set.active() == 0);
}
