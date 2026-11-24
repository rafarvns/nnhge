// Interruptions (spec 023 FR-19): plain stack changes during each phase, the transition's pop on a stack the game
// emptied, the pop of the last scene (under run() and in a manual loop), shutdown and the end of run() in each phase.

#include "input/input_block.hpp"
#include "nnh/input.hpp"
#include "scene/scene_hook.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;
using tr::Phase;

namespace {

Probe* g_c = nullptr;

} // namespace

NNH_TEST(plain_changes_during_a_transition_apply_as_usual) {
    for (int at = 0; at < 3; ++at) { // during out, covered and in
        restart();
        Probe a, b, c;
        nnh::scenes::push(a);
        step();
        CHECK(tr::replace(b, {.out = Frames{2}, .hold = Frames{2}, .in = Frames{2}}));
        step(at * 2 + 1);
        nnh::scenes::push(c);
        step();
        CHECK(nnh::scenes::top() == &c);
        run_until_done();
        // During out, the transition's replace then replaces the top of that moment (c); later, c stays on top.
        CHECK(at == 0 ? nnh::scenes::top() == &b && !c.on_stack() : nnh::scenes::top() == &c);
        nnh::shutdown();
    }
}

NNH_TEST(the_transitions_pop_of_a_stack_the_game_emptied_pops_nothing) {
    restart();
    Probe a;
    nnh::scenes::push(a);
    step();
    CHECK(tr::pop({.out = Frames{3}, .in = Frames{3}}));
    step();
    nnh::scenes::pop(); // the game empties the stack meanwhile
    LogCount logged("nothing to pop");
    CHECK(run_until_done() > 0);
    CHECK(nnh::scenes::size() == 0 && logged.count == 1);
    nnh::shutdown();
}

NNH_TEST(popping_the_last_scene_ends_run_after_the_covered_phase) {
    restart();
    Probe a;
    a.on_update = [](Probe&) { CHECK(tr::pop({.out = Frames{2}, .hold = Frames{3}, .in = Frames{10}})); };
    nnh::run(a);
    CHECK(a.updates == 1 && a.exits == 1 && !tr::running());
    CHECK(nnh::frame_count() == 1 + 2 + 3 + 1); // first update, out, covered (no in phase), the frame run() ends on
    nnh::shutdown();
}

NNH_TEST(popping_the_last_scene_in_a_manual_loop) {
    restart();
    Probe a;
    nnh::scenes::push(a);
    step();
    CHECK(tr::pop({.out = Frames{1}, .in = Frames{5}}));
    int frames = 0;
    while (nnh::frame() && (nnh::scenes::size() > 0 || tr::running())) {
        nnh::scenes::update_and_draw();
        ++frames;
    }
    CHECK(frames == 2 && nnh::scenes::size() == 0);
    nnh::shutdown();
}

NNH_TEST(shutdown_in_each_phase_cancels_silently) {
    for (int frames = 0; frames < 6; ++frames) { // before the first frame, then in each of the six frames' gaps
        restart();
        static int notified = 0;
        notified = 0;
        Probe a, b;
        nnh::scenes::push(a);
        step();
        CHECK(tr::replace(b, {.out = Frames{2}, .hold = Frames{2}, .in = Frames{2},
                              .on_finished = [] { ++notified; }}));
        step(frames);
        nnh::shutdown();
        CHECK(!tr::running() && notified == 0 && nnh::detail::g_scene_transition == nullptr);
        CHECK(nnh::detail::scene_pending_count(b) == 0 && !b.on_stack() && !a.on_stack());
        CHECK(!nnh::input::blocked());
    }
}

NNH_TEST(the_end_of_run_cancels_a_running_transition) {
    restart();
    static int finished = 0;
    finished = 0;
    Probe a, b;
    g_c = &b;
    // The system asks to quit on the tenth out frame (from the custom effect, called every transition frame).
    a.on_update = [](Probe& self) {
        if (self.updates != 1) return;
        CHECK(tr::replace(*g_c, {.effect = tr::custom(),
                                 .out = Frames{50},
                                 .custom = [](const tr::CustomFrame& f) {
                                     if (f.frame == 10) fake::set_quit_requested(true);
                                 },
                                 .on_finished = [] { ++finished; }}));
    };
    nnh::run(a);
    CHECK(!tr::running() && finished == 0 && nnh::detail::g_scene_transition == nullptr);
    CHECK(a.exits == 1 && b.enters == 0 && nnh::detail::scene_pending_count(b) == 0);
    CHECK(!nnh::input::blocked());
    nnh::shutdown();
}
