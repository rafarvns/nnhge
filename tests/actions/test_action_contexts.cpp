// Contexts switched by hand (spec 017 T2.5, FR-4): changes apply from the next evaluation, a disabled context
// reads idle with no released edge, two enabled contexts both see a shared control.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump, Confirm };
enum class Ctx { Play, Menu };

ActionSet make() {
    ActionSet set{{.actions = 2, .contexts = 2}};
    CHECK(set.add_context(Ctx::Menu, "menu"));
    CHECK(set.add(Act::Jump, {.name = "jump", .context = Ctx::Play, .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu, .bindings = {btn(Button::A)}}));
    step(set, 0);
    return set;
}
} // namespace

NNH_TEST(two_enabled_contexts_both_see_a_shared_control) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    CHECK(set.pressed(Act::Jump) && set.pressed(Act::Confirm));
    nnh::shutdown();
}

NNH_TEST(a_disabled_context_reads_idle_without_a_released_edge) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    step(set, bit(Button::A));
    set.enable(Ctx::Play, false);
    CHECK(!set.enabled(Ctx::Play));
    CHECK(set.held(Act::Jump)); // this frame's values stay until the next evaluation
    step(set, bit(Button::A));
    CHECK(!set.held(Act::Jump) && !set.released(Act::Jump) && !set.pressed(Act::Jump));
    CHECK(set.value(Act::Jump) == 0.0f && set.held_frames(Act::Jump) == 0);
    CHECK(set.frames_since_pressed(Act::Jump) == UINT32_MAX); // counters restart
    CHECK(set.held(Act::Confirm));
    step(set, 0);
    CHECK(!set.released(Act::Jump) && set.released(Act::Confirm));
    nnh::shutdown();
}

NNH_TEST(enabling_a_context_applies_from_the_next_evaluation) {
    restart();
    ActionSet set = make();
    set.enable(Ctx::Play, false);
    step(set, 0);
    set.enable(Ctx::Play, true);
    CHECK(set.enabled(Ctx::Play));
    step(set, bit(Button::A)); // held on the evaluation that enables it: blocked (FR-13)
    CHECK(!set.pressed(Act::Jump));
    step(set, 0);
    step(set, bit(Button::A));
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(a_context_declared_disabled_starts_idle) {
    restart();
    ActionSet set{{.actions = 1, .contexts = 2}};
    CHECK(set.add_context(Ctx::Menu, "menu", false));
    CHECK(set.add(Act::Jump, {.name = "jump", .context = Ctx::Menu, .bindings = {btn(Button::A)}}));
    step(set, 0);
    step(set, bit(Button::A));
    CHECK(!set.pressed(Act::Jump));
    nnh::shutdown();
}
