// Re-entrancy (spec 015 FR-12; T3.3): callbacks start, pause, complete and kill tweens of the same set
// (themselves included); started tweens count from the next update; killed tweens are not updated or called
// again; a chain of completions stops calling back at the depth limit with one logged error.

#include "tween_fixture.hpp"

using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;
using tween_test::run;

namespace {

struct Chain {
    TweenSet* set = nullptr;
    TweenHandle handles[12] = {};
    float values[12] = {};
    int called = 0;
};

Chain g_chain;

/// What a scene would hold: the set, its values and handles; callbacks capture a pointer to it.
struct Scene {
    TweenSet set{{.tweens = 8}};
    TweenHandle h{}, hb{};
    float x = 0, b = 0, d = 0;
    int loops = 0, finishes = 0;
};

void complete_next() {
    const int me = g_chain.called++;
    if (me + 1 < 12) g_chain.set->complete(g_chain.handles[me + 1]);
}

TweenSet* g_set = nullptr;
void update_from_inside() {
    float x = 0;
    g_set->to(&x, 1.0f, Frames{1}, {.on_finish = [] { g_set->update(); }});
    g_set->update();
}

} // namespace

NNH_TEST(a_tween_started_from_a_callback_counts_from_the_next_update) {
    TweenSet set({.tweens = 4});
    float a = 0, b = 0;
    set.to(&a, 1.0f, Frames{1}, {.on_finish = [&set, &b] { set.to(&b, 2.0f, Frames{2}); }});
    set.update(); // a finishes and starts b, which is not advanced in this update
    CHECK(a == 1.0f && b == 0.0f);
    set.update();
    CHECK(b == 1.0f);
    set.update();
    CHECK(b == 2.0f);
}

NNH_TEST(a_from_value_started_from_a_callback_is_written_at_once) {
    TweenSet set({.tweens = 4});
    float a = 0, b = 0;
    set.to(&a, 1.0f, Frames{1}, {.on_finish = [&set, &b] { set.from_to(&b, 5.0f, 6.0f, Frames{2}); }});
    set.update();
    CHECK(b == 5.0f);
}

NNH_TEST(a_tween_killed_by_an_earlier_callback_is_not_updated_or_called) {
    TweenSet set({.tweens = 4});
    float a = 0, b = 0;
    int b_finished = 0;
    TweenHandle hb{};
    set.to(&a, 1.0f, Frames{1}, {.on_finish = [&set, &hb] { set.kill(hb); }});
    hb = set.to(&b, 1.0f, Frames{1}, {.on_finish = [&b_finished] { ++b_finished; }});
    set.update();
    CHECK(b == 0.0f && b_finished == 0);
    CHECK(!set.running(hb) && set.active() == 0);
}

NNH_TEST(a_tween_can_kill_itself_from_on_loop) {
    Scene s;
    s.h = s.set.to(&s.x, 4.0f, Frames{4},
                   {.loops = 5,
                    .on_finish = [p = &s] { ++p->finishes; },
                    .on_loop =
                        [p = &s] {
                            ++p->loops;
                            p->set.kill(p->h);
                        }});
    s.set.advance(Frames{12}); // three loops would end; the first kills it
    CHECK(s.loops == 1 && s.finishes == 0);
    CHECK(!s.set.running(s.h));
    CHECK(s.x == 0.0f); // killed before this advance wrote any value: the value stays where it was
}

NNH_TEST(completing_a_tween_from_its_own_on_finish_does_nothing_more) {
    Scene s;
    s.h = s.set.to(&s.x, 1.0f, Frames{1}, {.on_finish = [p = &s] {
        ++p->finishes;
        p->set.complete(p->h);
        p->set.kill(p->h);
    }});
    s.set.update();
    CHECK(s.finishes == 1);
    s.set.complete(s.h);
    CHECK(s.finishes == 1);
}

NNH_TEST(a_callback_can_pause_complete_and_kill_others) {
    Scene s;
    float a = 0;
    s.set.to(&a, 1.0f, Frames{2}, {.on_finish = [p = &s] {
        p->set.pause(p->hb);
        p->set.complete(p->h);
        p->set.kill_all();
        p->set.to(&p->d, 1.0f, Frames{1}); // after kill_all: lives
    }});
    s.hb = s.set.to(&s.b, 1.0f, Frames{4});
    s.h = s.set.to(&s.x, 1.0f, Frames{4}, {.on_finish = [p = &s] { ++p->finishes; }});
    run(s.set, 2);
    CHECK(s.x == 1.0f && s.finishes == 1);
    CHECK(s.b == 0.25f); // killed when `a` finished, before its own advance in that update
    CHECK(!s.set.running(s.hb) && s.set.active() == 1);
    run(s.set, 1);
    CHECK(s.d == 1.0f && s.set.active() == 0);
}

NNH_TEST(a_chain_of_completions_stops_calling_back_at_the_depth_limit) {
    tween_test::restart();
    {
        TweenSet set({.tweens = 12});
        g_chain = Chain{};
        g_chain.set = &set;
        for (int i = 0; i < 12; ++i) {
            g_chain.handles[i] = set.to(&g_chain.values[i], 1.0f, Frames{100}, {.on_finish = complete_next});
        }
        const int lines = nnh::platform_fake::state().console_lines;
        set.complete(g_chain.handles[0]);
        CHECK(g_chain.called == nnh::kMaxCallbackDepth);
        // Callbacks ran for the first 8; the 8th completed the 9th, silently, which ended the chain.
        for (int i = 0; i < 12; ++i) {
            CHECK(g_chain.values[i] == (i <= nnh::kMaxCallbackDepth ? 1.0f : 0.0f));
        }
        CHECK(set.active() == 12 - nnh::kMaxCallbackDepth - 1);
        CHECK(nnh::platform_fake::state().console_lines == lines + 1);
        CHECK(tween_test::last_log_mentions("depth limit"));
    }
    nnh::shutdown();
}

NNH_TEST(updating_a_set_from_its_own_callback_asserts_or_is_ignored) {
    TweenSet set({.tweens = 4});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(update_from_inside));
    tween_test::recover_after_crash(set); // the assert left the update half done
}
