// Completion (spec 015 FR-8, FR-11; T3.1): on_finish after the final write, on_loop, the game's event
// delivered at the next nnh::frame(), nothing on kill, kill_all or destruction; complete() runs the
// completion once.

#include "nnh/events.hpp"
#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::LoopMode;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

namespace {

struct IntroFinished {
    int id;
};
struct FromCallback {
    int id;
};

struct Log {
    int finishes = 0;
    int loops = 0;
    float value_seen = -1;
    int order[4] = {};
    int count = 0;
};

} // namespace

NNH_TEST(on_finish_runs_in_the_finishing_update_after_the_final_write) {
    TweenSet set({.tweens = 4});
    float x = 0;
    Log log;
    set.to(&x, 3.0f, Frames{3}, {.on_finish = [&log, &x] {
        ++log.finishes;
        log.value_seen = x;
    }});
    run(set, 2);
    CHECK(log.finishes == 0);
    run(set, 1);
    CHECK(log.finishes == 1);
    CHECK(log.value_seen == 3.0f);
    run(set, 5);
    CHECK(log.finishes == 1);
}

NNH_TEST(on_loop_runs_at_the_end_of_every_loop_but_the_last) {
    TweenSet set({.tweens = 4});
    float x = 0;
    Log log;
    set.to(&x, 1.0f, Frames{2},
           {.loops = 4,
            .loop_mode = LoopMode::Yoyo,
            .on_finish = [&log] { ++log.finishes; },
            .on_loop = [&log] { ++log.loops; }});
    run(set, 2);
    CHECK(log.loops == 1);
    run(set, 6);
    CHECK(log.loops == 3 && log.finishes == 1);
}

NNH_TEST(on_loop_runs_once_per_loop_even_when_several_end_in_one_advance) {
    TweenSet set({.tweens = 4});
    float x = 0;
    Log log;
    set.to(&x, 1.0f, Frames{1}, {.loops = nnh::kForever, .on_loop = [&log] { ++log.loops; }});
    set.advance(Frames{7});
    CHECK(log.loops == 7);
}

NNH_TEST(a_tween_paused_by_its_on_loop_stops_ending_loops_in_that_update) {
    struct Ctx {
        TweenSet set{{.tweens = 2}};
        nnh::TweenHandle h{};
        float x = 0;
        int loops = 0;
    } c;
    c.h = c.set.to(&c.x, 1.0f, Frames{1}, {.loops = 10, .on_loop = [p = &c] {
                                               ++p->loops;
                                               p->set.pause(p->h);
                                           }});
    c.set.advance(Frames{5});
    CHECK(c.loops == 1);
    CHECK(c.set.paused(c.h) && c.set.loops_left(c.h) == 9);
}

NNH_TEST(the_finish_event_is_delivered_at_the_next_frame) {
    tween_test::restart();
    {
        TweenSet set({.tweens = 4});
        float x = 0;
        int received = 0, id = 0;
        nnh::Subscription sub = nnh::events::subscribe<IntroFinished>([&](const IntroFinished& e) {
            ++received;
            id = e.id;
        });
        set.to(&x, 1.0f, Frames{2}, {.publish = IntroFinished{7}});
        run(set, 2);
        CHECK(received == 0); // queued, not delivered inside the update
        CHECK(nnh::frame());
        CHECK(received == 1 && id == 7);
        CHECK(nnh::frame());
        CHECK(received == 1);
    }
    nnh::shutdown();
}

NNH_TEST(on_finish_runs_before_the_event_is_queued) {
    tween_test::restart();
    {
        TweenSet set({.tweens = 4});
        float x = 0;
        Log log;
        nnh::Subscription a =
            nnh::events::subscribe<FromCallback>([&log](const FromCallback&) { log.order[log.count++] = 1; });
        nnh::Subscription b = nnh::events::subscribe<IntroFinished>(
            [&log](const IntroFinished&) { log.order[log.count++] = 2; });
        set.to(&x, 1.0f, Frames{1},
               {.on_finish = [] { nnh::events::publish(FromCallback{1}); }, .publish = IntroFinished{1}});
        set.update();
        CHECK(nnh::frame());
        CHECK(log.count == 2 && log.order[0] == 1 && log.order[1] == 2);
    }
    nnh::shutdown();
}

NNH_TEST(kill_kill_all_and_destruction_run_no_callback_and_publish_nothing) {
    tween_test::restart();
    {
        int received = 0;
        nnh::Subscription sub =
            nnh::events::subscribe<IntroFinished>([&](const IntroFinished&) { ++received; });
        Log log;
        float x = 0;
        const nnh::TweenOptions options{.on_finish = [&log] { ++log.finishes; },
                                        .on_loop = [&log] { ++log.loops; },
                                        .publish = IntroFinished{1}};
        {
            TweenSet set({.tweens = 4});
            const TweenHandle h = set.to(&x, 1.0f, Frames{4}, options);
            set.update();
            set.kill(h);
            set.to(&x, 1.0f, Frames{4}, options);
            set.to(&x, 1.0f, Frames{4}, options);
            set.kill_all();
            set.to(&x, 1.0f, Frames{4}, options);
            set.update();
        } // destroyed with a tween still playing
        CHECK(nnh::frame());
        CHECK(log.finishes == 0 && log.loops == 0 && received == 0);
    }
    nnh::shutdown();
}

NNH_TEST(complete_runs_on_finish_and_the_event_once_but_not_on_loop) {
    tween_test::restart();
    {
        TweenSet set({.tweens = 4});
        int received = 0;
        nnh::Subscription sub =
            nnh::events::subscribe<IntroFinished>([&](const IntroFinished&) { ++received; });
        Log log;
        float x = 0;
        const TweenHandle h = set.to(&x, 1.0f, Frames{4},
                                     {.loops = 5,
                                      .on_finish = [&log] { ++log.finishes; },
                                      .on_loop = [&log] { ++log.loops; },
                                      .publish = IntroFinished{1}});
        set.update();
        set.complete(h);
        set.complete(h);
        CHECK(log.finishes == 1 && log.loops == 0);
        CHECK(nnh::frame());
        CHECK(received == 1);
    }
    nnh::shutdown();
}

NNH_TEST(a_tween_can_publish_an_empty_event) {
    struct Done {};
    tween_test::restart();
    {
        TweenSet set({.tweens = 1});
        int received = 0;
        nnh::Subscription sub = nnh::events::subscribe<Done>([&](const Done&) { ++received; });
        float x = 0;
        set.to(&x, 1.0f, Frames{1}, {.publish = Done{}});
        set.update();
        CHECK(nnh::frame());
        CHECK(received == 1);
    }
    nnh::shutdown();
}
