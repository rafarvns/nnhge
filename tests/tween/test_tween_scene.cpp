// Scene-bound tween sets (spec 015 FR-15, AC-3; T5.3): updated right before their scene's update(), only
// while the scene is on top, cleared silently when it leaves the stack (pop, replace, end of run(),
// shutdown()), working again after the scene re-enters; updating a bound set by hand asserts.

#include "nnh/events.hpp"
#include "nnh/scene.hpp"
#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::TweenSet;

namespace {

struct Finished {};

/// A scene with a bound set; on_enter starts a 60-frame tween of `x` (and, optionally, a callback and an
/// event).
class Animated : public nnh::Scene {
  public:
    void on_enter() override {
        tweens.to(&x, 60.0f, Frames{60}, {.on_finish = [this] { ++finishes; }, .publish = Finished{}});
    }
    void on_exit() override {
        if (start_in_on_exit) tweens.after(Frames{1}, [this] { ++finishes; });
    }
    void update() override {
        seen = x;
        ++updates;
        if (pops_after > 0 && updates == pops_after) nnh::scenes::pop();
    }

    TweenSet tweens{*this, {.tweens = 8}};
    float x = 0, seen = -1;
    int updates = 0, finishes = 0, pops_after = 0;
    bool start_in_on_exit = false;
};

/// Runs `n` frames of the scene stack.
void frames(int n) {
    for (int i = 0; i < n; ++i) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
    }
}

Animated* g_scene = nullptr;
void update_bound_set_by_hand() {
    g_scene->tweens.update();
}
void advance_bound_set_by_hand() {
    g_scene->tweens.advance(Frames{1});
}

} // namespace

NNH_TEST(a_bound_set_updates_right_before_its_scenes_update) {
    tween_test::restart();
    {
        Animated scene;
        CHECK(scene.tweens.bound());
        nnh::scenes::push(scene);
        frames(1);
        CHECK(scene.seen == 1.0f); // this frame's value, already in update()
        frames(9);
        CHECK(scene.seen == 10.0f && scene.x == 10.0f);
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_set_freezes_under_a_pushed_scene_whose_own_set_plays) {
    tween_test::restart();
    {
        Animated level, pause;
        nnh::scenes::push(level);
        frames(5);
        nnh::scenes::push(pause);
        frames(10);
        CHECK(level.x == 5.0f);  // frozen under the pause menu
        CHECK(pause.x == 10.0f); // it entered at the start of the first of those frames
        nnh::scenes::pop();
        frames(2);
        CHECK(level.x == 7.0f); // the pop applies at the start of the next frame
        nnh::shutdown();
    }
}

NNH_TEST(popping_a_scene_clears_its_set_silently) {
    tween_test::restart();
    {
        int received = 0;
        nnh::Subscription sub = nnh::events::subscribe<Finished>([&](const Finished&) { ++received; });
        Animated base, top;
        top.start_in_on_exit = true; // started in on_exit(): cleared too
        nnh::scenes::push(base);
        nnh::scenes::push(top);
        frames(3);
        nnh::scenes::pop();
        frames(100);
        CHECK(top.x == 3.0f); // untouched after it left (3 updates while on top)
        CHECK(top.finishes == 0 && top.tweens.active() == 0);
        CHECK(base.x == 60.0f && base.finishes == 1);
        CHECK(received == 1); // only the base scene's tween finished
        nnh::shutdown();
    }
}

NNH_TEST(replace_and_shutdown_clear_bound_sets) {
    tween_test::restart();
    {
        Animated a, b;
        nnh::scenes::push(a);
        frames(3);
        nnh::scenes::replace(b);
        frames(3);
        CHECK(a.tweens.active() == 0 && a.x == 3.0f);
        CHECK(b.tweens.active() == 1);
        nnh::shutdown(); // b leaves the stack: its set is cleared, nothing is called back
        CHECK(b.tweens.active() == 0 && b.finishes == 0);
    }
}

NNH_TEST(the_end_of_run_clears_the_last_scenes_set) {
    tween_test::restart();
    {
        Animated scene;
        scene.pops_after = 5;
        nnh::run(scene);
        CHECK(scene.updates == 5 && scene.x == 5.0f);
        CHECK(scene.tweens.active() == 0 && scene.finishes == 0);
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_set_plays_again_after_its_scene_re_enters) {
    tween_test::restart();
    {
        Animated base, again;
        nnh::scenes::push(base);
        nnh::scenes::push(again);
        frames(4);
        nnh::scenes::pop();
        frames(1);
        nnh::scenes::push(again); // on_enter starts the tween again; it captures x = 4
        frames(3);
        CHECK(again.x == 4.0f + (60.0f - 4.0f) * (12288.0f / 245760.0f));
        CHECK(again.tweens.active() == 1);
        nnh::shutdown();
    }
}

NNH_TEST(a_bound_set_replaced_while_on_the_stack_detaches) {
    tween_test::restart();
    {
        Animated scene;
        nnh::scenes::push(scene);
        frames(2);
        scene.tweens = TweenSet{}; // the bound block goes; the scene stays
        frames(2);
        CHECK(scene.x == 2.0f && !scene.tweens.bound());
        nnh::shutdown();
    }
}

NNH_TEST(updating_a_bound_set_by_hand_asserts_or_is_ignored) {
    tween_test::restart();
    {
        Animated scene;
        nnh::scenes::push(scene);
        frames(1);
        g_scene = &scene;
        CHECK(tween_test::asserts_or_is_ignored(update_bound_set_by_hand));
        CHECK(tween_test::asserts_or_is_ignored(advance_bound_set_by_hand));
        CHECK(scene.x == 1.0f);
        nnh::shutdown();
    }
}

NNH_TEST(an_unbound_set_is_not_bound) {
    TweenSet set({.tweens = 1});
    CHECK(!set.bound());
    CHECK(!TweenSet{}.bound());
}
