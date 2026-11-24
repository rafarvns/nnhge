// What the scenes do in each phase (spec 023 FR-10): no update by default, the update options, the incoming scene's
// first update the frame after done, D30's BeforeUpdate/AfterExit around the scenes' own calls.

#include "nnh/tween.hpp"
#include "transition/transition_internal.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

NNH_TEST(no_scene_updates_during_a_transition_by_default) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    const int a_updates = a.updates;
    CHECK(tr::replace(b, {.out = Frames{3}, .hold = Frames{2}, .in = Frames{3}}));
    const std::uint32_t started = frame_no() + 1;
    CHECK(run_until_done() == 8);
    CHECK(a.updates == a_updates && b.updates == 0);
    CHECK(a.draws > 0 && b.draws > 0); // both draw, under the cover
    step();
    CHECK(b.updates == 1 && b.first_update == started + 8); // the frame after done
    nnh::shutdown();
}

NNH_TEST(update_out_and_update_in_let_the_top_scene_update) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    const int a_updates = a.updates;
    CHECK(tr::replace(b, {.out = Frames{3}, .hold = Frames{2}, .in = Frames{4}, .update_out = true,
                          .update_in = true}));
    CHECK(run_until_done() == 9);
    CHECK(a.updates == a_updates + 3); // every out frame
    CHECK(b.updates == 4);             // every in frame, none while covered
    nnh::shutdown();
}

NNH_TEST(a_bound_tween_set_freezes_while_its_scene_does_not_update) {
    restart();
    struct Animated : Probe {
        nnh::TweenSet tweens{*this, {.tweens = 2}};
        float x = 0;
        void on_enter() override {
            Probe::on_enter();
            tweens.to(&x, 100.0f, Frames{100});
        }
    } a;
    Probe b;
    nnh::scenes::push(a);
    step(2);
    const float before = a.x;
    CHECK(before > 0.0f);
    CHECK(tr::push(b, {.out = Frames{5}}));
    step(4);
    CHECK(a.x == before); // frozen with its scene
    nnh::detail::transition_cancel();
    nnh::shutdown();
}

NNH_TEST(after_exit_runs_after_the_outgoing_scenes_on_exit) {
    restart();
    struct Animated : Probe {
        nnh::TweenSet tweens{*this, {.tweens = 2}};
        float x = 0;
        void on_exit() override {
            Probe::on_exit();
            active_at_exit = tweens.active();
        }
        int active_at_exit = -1;
    } a;
    Probe b;
    nnh::scenes::push(a);
    step();
    a.tweens.to(&a.x, 1.0f, Frames{1000});
    CHECK(tr::replace(b, {.out = Frames{2}, .in = Frames{2}}));
    run_until_done();
    CHECK(a.active_at_exit == 1 && a.tweens.active() == 0); // cleared right after on_exit (D30)
    nnh::shutdown();
}
