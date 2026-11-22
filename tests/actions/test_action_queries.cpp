// Queries and edges (spec 017 T2.3, FR-11): pressed/held/released, held_frames, frames_since_pressed, a control
// held when the set is created doesn't press.

#include "actions_fixture.hpp"

#include <cstdint>

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump, Fire };

ActionSet* g_set = nullptr;
void query_undeclared() { (void)g_set->held(Act::Fire); }
void query_out_of_range() { (void)g_set->pressed(nnh::ActionId(9)); }

ActionSet make() {
    ActionSet set{{.actions = 2}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::B)}}));
    step(set, 0); // the first evaluation (nothing held)
    return set;
}
} // namespace

NNH_TEST(an_action_reports_one_frame_edges_and_held) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    CHECK(set.pressed(Act::Jump) && set.held(Act::Jump) && !set.released(Act::Jump));
    step(set, bit(Button::A));
    CHECK(!set.pressed(Act::Jump) && set.held(Act::Jump));
    step(set, 0);
    CHECK(set.released(Act::Jump) && !set.held(Act::Jump) && !set.pressed(Act::Jump));
    step(set, 0);
    CHECK(!set.released(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(switching_between_two_held_bindings_is_not_a_new_press) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    step(set, bit(Button::A) | bit(Button::B));
    step(set, bit(Button::B));
    CHECK(set.held(Act::Jump) && !set.pressed(Act::Jump) && !set.released(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(held_frames_counts_from_one_on_the_press_frame) {
    restart();
    ActionSet set = make();
    CHECK(set.held_frames(Act::Jump) == 0);
    step(set, bit(Button::A));
    CHECK(set.held_frames(Act::Jump) == 1);
    step(set, bit(Button::A));
    step(set, bit(Button::A));
    CHECK(set.held_frames(Act::Jump) == 3);
    step(set, 0);
    CHECK(set.held_frames(Act::Jump) == 0);
    nnh::shutdown();
}

NNH_TEST(frames_since_pressed_is_never_until_a_press_then_counts) {
    restart();
    ActionSet set = make();
    CHECK(set.frames_since_pressed(Act::Jump) == UINT32_MAX);
    step(set, bit(Button::A));
    CHECK(set.frames_since_pressed(Act::Jump) == 0);
    step(set, 0);
    step(set, 0);
    CHECK(set.frames_since_pressed(Act::Jump) == 2);
    step(set, bit(Button::B));
    CHECK(set.frames_since_pressed(Act::Jump) == 0);
    nnh::shutdown();
}

NNH_TEST(frames_since_pressed_saturates) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    block_of(set)->actions[0].since_pressed = UINT32_MAX - 1;
    step(set, 0);
    step(set, 0);
    CHECK(set.frames_since_pressed(Act::Jump) == UINT32_MAX);
    block_of(set)->actions[0].held_frames = UINT32_MAX;
    nnh::shutdown();
}

NNH_TEST(held_frames_saturates) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::A));
    block_of(set)->actions[0].held_frames = UINT32_MAX;
    step(set, bit(Button::A));
    CHECK(set.held_frames(Act::Jump) == UINT32_MAX);
    nnh::shutdown();
}

NNH_TEST(a_control_held_when_the_set_is_created_does_not_press) {
    restart();
    ActionSet set{{.actions = 1}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    step(set, bit(Button::A));
    CHECK(!set.pressed(Act::Jump) && !set.held(Act::Jump));
    step(set, bit(Button::A));
    CHECK(!set.held(Act::Jump));
    step(set, 0);
    CHECK(!set.released(Act::Jump));
    step(set, bit(Button::A));
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(querying_an_unknown_action_asserts_or_reads_idle) {
    restart();
    ActionSet set = make();
    g_set = &set;
    CHECK(asserts_or_is_ignored(query_undeclared));
    CHECK(asserts_or_is_ignored(query_out_of_range));
    nnh::shutdown();
}
