// The stack change (spec 023 FR-11, FR-14): spec 001's deferred path at the end of the last out frame, a slow
// on_enter() skipping no frame, a waiting scene's ownership checks, a loading scene starting a second transition.

#include <new>

#include "transition/transition_internal.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;

namespace {

alignas(Probe) unsigned char g_storage[sizeof(Probe)];
Probe* g_waiting = nullptr;
Probe* g_other = nullptr;

void destroy_the_waiting_scene() { g_waiting->~Probe(); }

void push_the_waiting_scene_elsewhere_then_run() {
    nnh::scenes::push(*g_waiting);
    run_until_done();
}

} // namespace

NNH_TEST(the_change_happens_after_the_last_out_frame) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{3}, .in = Frames{2}}));
    step(3);
    CHECK(nnh::scenes::top() == &a && a.exits == 0 && b.enters == 0); // three out frames: still the old scene
    const std::uint32_t last_out = frame_no();
    step();
    CHECK(nnh::scenes::top() == &b && a.exits == 1 && b.enters == 1);
    CHECK(a.exit_frame == last_out && b.enter_frame == last_out); // applied as the next frame starts
    CHECK(tr::phase() == tr::Phase::In); // its one covered frame ended: the next frame starts the in phase
    run_until_done();
    nnh::shutdown();
}

NNH_TEST(a_slow_on_enter_skips_no_transition_frame) {
    restart();
    struct Slow : Probe {
        void on_enter() override {
            Probe::on_enter();
            fake::advance_time(nnh::Duration::from_seconds(3)); // a long load
        }
    } b;
    Probe a;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{4}, .hold = Frames{3}, .in = Frames{5}}));
    CHECK(run_until_done() == 4 + 3 + 5);
    nnh::shutdown();
}

NNH_TEST(destroying_the_waiting_scene_is_caught) {
    restart();
    Probe a;
    nnh::scenes::push(a);
    step();
    g_waiting = new (g_storage) Probe();
    CHECK(tr::replace(*g_waiting, {.out = Frames{4}}));
    CHECK(crashes(destroy_the_waiting_scene)); // ~Scene: still waiting to be pushed
    nnh::detail::transition_cancel();
    nnh::shutdown();
}

NNH_TEST(pushing_the_waiting_scene_elsewhere_is_caught) {
    restart();
    Probe a, b;
    g_waiting = &b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::push(b, {.out = Frames{2}}));
    CHECK(crashes(push_the_waiting_scene_elsewhere_then_run)); // the transition's push finds it on the stack
    nnh::detail::transition_cancel();
    nnh::shutdown();
}

NNH_TEST(a_loading_scene_starts_the_second_transition_from_its_update) {
    restart();
    Probe title, loading, level;
    g_other = &level;
    loading.on_update = [](Probe& self) {
        if (self.updates == 5) CHECK(tr::replace(*g_other, {.out = Frames{2}, .in = Frames{2}}));
    };
    nnh::scenes::push(title);
    step();
    CHECK(tr::replace(loading, {.out = Frames{2}, .in = Frames{2}}));
    CHECK(run_until_done() == 5);
    step(5); // the loading scene's slices; the fifth starts the next transition
    CHECK(loading.updates == 5 && tr::running());
    CHECK(run_until_done() == 5);
    CHECK(nnh::scenes::top() == &level && loading.updates == 5 && level.updates == 0);
    step();
    CHECK(level.updates == 1);
    nnh::shutdown();
}
