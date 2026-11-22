// Combining slots (spec 017 T1.3, FR-7) and the axis/vector held hysteresis (FR-3).

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::detail::combine;
using nnh::detail::held_with_hysteresis;
using nnh::detail::SlotOut;

namespace {
SlotOut button(bool held, float value) { return {held, value, {value, 0}}; }
SlotOut axis(float value) { return {false, value, {value, 0}}; }
SlotOut vec(float x, float y, float length) { return {false, length, {x, y}}; }
} // namespace

NNH_TEST(a_button_action_is_held_when_any_slot_is_and_takes_the_largest_value) {
    SlotOut out;
    combine(ActionKind::Button, out, button(false, 0.3f));
    combine(ActionKind::Button, out, button(true, 1.0f));
    combine(ActionKind::Button, out, button(false, 0.45f));
    CHECK(out.held && out.value == 1.0f);
    SlotOut none;
    combine(ActionKind::Button, none, button(false, 0.3f));
    combine(ActionKind::Button, none, button(false, 0.2f));
    CHECK(!none.held && none.value == 0.3f);
}

NNH_TEST(an_axis_takes_the_slot_with_the_largest_absolute_value) {
    SlotOut out;
    combine(ActionKind::Axis, out, axis(0.4f));
    combine(ActionKind::Axis, out, axis(-0.9f));
    combine(ActionKind::Axis, out, axis(0.5f));
    CHECK(out.value == -0.9f && out.vec.x == -0.9f);
}

NNH_TEST(an_axis_tie_keeps_the_lowest_slot) {
    SlotOut out;
    combine(ActionKind::Axis, out, axis(-1.0f));
    combine(ActionKind::Axis, out, axis(1.0f));
    CHECK(out.value == -1.0f);
}

NNH_TEST(a_vector_takes_the_longest_slot_and_ties_keep_the_lowest) {
    SlotOut out;
    combine(ActionKind::Vector, out, vec(0.3f, 0, 0.3f));
    combine(ActionKind::Vector, out, vec(0, -0.8f, 0.8f));
    combine(ActionKind::Vector, out, vec(0.8f, 0, 0.8f));
    CHECK(out.vec.y == -0.8f && out.vec.x == 0.0f && out.value == 0.8f);
}

NNH_TEST(the_same_control_in_two_slots_is_harmless) {
    SlotOut out;
    combine(ActionKind::Button, out, button(true, 1.0f));
    combine(ActionKind::Button, out, button(true, 1.0f));
    CHECK(out.held && out.value == 1.0f);
}

NNH_TEST(axis_and_vector_held_use_press_and_release_thresholds) {
    CHECK(!held_with_hysteresis(0.49f, false, 0.5f, 0.4f));
    CHECK(held_with_hysteresis(0.5f, false, 0.5f, 0.4f));
    CHECK(held_with_hysteresis(0.4f, true, 0.5f, 0.4f));
    CHECK(!held_with_hysteresis(0.39f, true, 0.5f, 0.4f));
}
