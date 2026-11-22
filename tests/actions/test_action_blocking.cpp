// No leaking presses (spec 017 T2.5, FR-13): when a context is enabled, each slot whose control is held is
// blocked until that control is released; other slots of the action still work.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Source;

namespace {
enum class Act { Jump, Move };
enum class Ctx { Play };

ActionSet make() {
    ActionSet set{{.actions = 2}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::B)}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}));
    step(set, 0);
    return set;
}

void off_then_on(ActionSet& set, std::uint32_t held, nnh::Stick pad = {}) {
    set.enable(Ctx::Play, false);
    step(set, held, pad);
    set.enable(Ctx::Play, true);
}
} // namespace

NNH_TEST(a_button_held_when_its_context_is_enabled_is_blocked_until_released) {
    restart();
    ActionSet set = make();
    off_then_on(set, bit(Button::A));
    step(set, bit(Button::A));
    CHECK(!set.held(Act::Jump) && !set.pressed(Act::Jump));
    step(set, bit(Button::A));
    CHECK(!set.held(Act::Jump));
    step(set, 0);
    CHECK(!set.released(Act::Jump));
    step(set, bit(Button::A));
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(another_slot_of_a_blocked_action_still_works) {
    restart();
    ActionSet set = make();
    off_then_on(set, bit(Button::A));
    step(set, bit(Button::A));
    step(set, bit(Button::A) | bit(Button::B));
    CHECK(set.pressed(Act::Jump));
    step(set, bit(Button::A));
    CHECK(set.released(Act::Jump)); // A is still blocked
    nnh::shutdown();
}

NNH_TEST(a_composite_stays_blocked_until_all_its_buttons_are_released) {
    restart();
    ActionSet set = make();
    off_then_on(set, bit(Button::Up) | bit(Button::Left));
    step(set, bit(Button::Up) | bit(Button::Left));
    step(set, bit(Button::Left));
    CHECK(!set.held(Act::Move) && set.vector(Act::Move).x == 0.0f);
    step(set, 0);
    step(set, bit(Button::Right));
    CHECK(set.pressed(Act::Move) && set.vector(Act::Move).x == 1.0f);
    nnh::shutdown();
}

NNH_TEST(a_stick_stays_blocked_until_below_its_release_threshold) {
    restart();
    ActionSet set = make();
    off_then_on(set, 0, {0.9f, 0});
    step(set, 0, {0.9f, 0});
    CHECK(!set.held(Act::Move) && set.value(Act::Move) == 0.0f);
    step(set, 0, {0.45f, 0}); // under press, over release: still blocked
    CHECK(set.value(Act::Move) == 0.0f);
    step(set, 0, {0.3f, 0}); // released: counts again (below the press threshold)
    CHECK(near(set.value(Act::Move), 0.3f) && !set.held(Act::Move));
    step(set, 0, {0.8f, 0});
    CHECK(set.pressed(Act::Move));
    nnh::shutdown();
}

NNH_TEST(a_stick_under_the_press_threshold_is_not_blocked) {
    restart();
    ActionSet set = make();
    off_then_on(set, 0, {0.45f, 0});
    step(set, 0, {0.45f, 0});
    CHECK(near(set.value(Act::Move), 0.45f));
    nnh::shutdown();
}
