// Action kinds through the set (spec 017 T2.3, FR-3): button, axis and vector values, held for analog kinds with
// the action's thresholds and hysteresis, value of a vector = its length.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Source;
using nnh::StickAxis;

namespace {
enum class Act { Fire, Aim, Move };

ActionSet make() {
    ActionSet set{{.actions = 3}};
    CHECK(set.add(Act::Fire, {.name = "fire",
                              .bindings = {btn(Button::A),
                                           {Source::stick_direction(AnalogStick::CStick, nnh::Direction::Up)}}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)},
                                          {Source::buttons(Button::L, Button::R)}},
                             .press = 0.6f,
                             .release = 0.3f}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}));
    step(set, 0);
    return set;
}
} // namespace

NNH_TEST(a_button_action_value_is_the_largest_slot_value) {
    restart();
    ActionSet set = make();
    step(set, 0, {}, {0, 0.3f});
    CHECK(!set.held(Act::Fire) && near(set.value(Act::Fire), 0.3f));
    step(set, bit(Button::A), {}, {0, 0.3f});
    CHECK(set.held(Act::Fire) && set.value(Act::Fire) == 1.0f);
    CHECK(set.vector(Act::Fire).x == 1.0f && set.vector(Act::Fire).y == 0.0f);
    nnh::shutdown();
}

NNH_TEST(an_axis_is_held_past_its_press_threshold_until_below_release) {
    restart();
    ActionSet set = make();
    step(set, 0, {}, {-0.55f, 0});
    CHECK(near(set.value(Act::Aim), -0.55f) && !set.held(Act::Aim));
    step(set, 0, {}, {-0.6f, 0});
    CHECK(set.held(Act::Aim) && set.pressed(Act::Aim));
    step(set, 0, {}, {0.31f, 0});
    CHECK(set.held(Act::Aim));
    step(set, 0, {}, {0.29f, 0});
    CHECK(!set.held(Act::Aim) && set.released(Act::Aim));
    step(set, bit(Button::L));
    CHECK(set.value(Act::Aim) == -1.0f && set.pressed(Act::Aim));
    nnh::shutdown();
}

NNH_TEST(a_vector_reads_its_longest_slot_and_its_value_is_the_length) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::Up), {0.3f, 0});
    CHECK(set.vector(Act::Move).y == 1.0f && set.vector(Act::Move).x == 0.0f);
    CHECK(set.value(Act::Move) == 1.0f && set.held(Act::Move) && set.pressed(Act::Move));
    step(set, 0, {0.3f, -0.4f});
    CHECK(near(set.vector(Act::Move).x, 0.3f) && near(set.vector(Act::Move).y, -0.4f));
    CHECK(near(set.value(Act::Move), 0.5f) && set.held(Act::Move));
    step(set, 0, {0.3f, 0});
    CHECK(!set.held(Act::Move) && set.released(Act::Move));
    nnh::shutdown();
}

NNH_TEST(a_vector_never_exceeds_length_one) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::Up) | bit(Button::Right), {1.0f, 0});
    const nnh::Vec2 v = set.vector(Act::Move);
    CHECK(v.x * v.x + v.y * v.y <= 1.0f + 1e-5f);
    nnh::shutdown();
}
