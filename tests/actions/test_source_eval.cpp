// One slot's reading for every source kind (spec 017 T1.2, FR-6): thresholds and hysteresis, dead-zone
// re-scale, composites, touch areas, invert.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::AnalogStick;
using nnh::Direction;
using nnh::Source;
using nnh::StickAxis;
using nnh::detail::eval_slot;
using nnh::detail::SlotOut;

NNH_TEST(a_button_source_is_held_with_value_one) {
    auto s = slot({Source::button(Button::B)});
    CHECK(eval_slot(ActionKind::Button, s, view(bit(Button::B))).held);
    CHECK(eval_slot(ActionKind::Button, s, view(bit(Button::B))).value == 1.0f);
    CHECK(!eval_slot(ActionKind::Button, s, view(bit(Button::A))).held);
    CHECK(eval_slot(ActionKind::Button, s, view(bit(Button::A))).value == 0.0f);
}

NNH_TEST(a_stick_direction_presses_at_half_and_lets_go_below_forty_percent) {
    auto s = slot({Source::stick_direction(AnalogStick::CirclePad, Direction::Right)});
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {0.49f, 0})).held);
    CHECK(eval_slot(ActionKind::Button, s, view(0, {0.5f, 0})).held);
    CHECK(eval_slot(ActionKind::Button, s, view(0, {0.41f, 0})).held); // hysteresis
    const SlotOut low = eval_slot(ActionKind::Button, s, view(0, {0.39f, 0}));
    CHECK(!low.held && near(low.value, 0.39f)); // value = push, even below the threshold
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {0.45f, 0})).held);
}

NNH_TEST(a_stick_direction_uses_its_own_thresholds) {
    auto s = slot({.source = Source::stick_direction(AnalogStick::CStick, Direction::Up), .press = 0.8f,
                   .release = 0.2f});
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {0, 0.7f})).held);
    CHECK(eval_slot(ActionKind::Button, s, view(0, {}, {0, 0.8f})).held);
    CHECK(eval_slot(ActionKind::Button, s, view(0, {}, {0, 0.25f})).held);
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {0, 0.1f})).held);
}

NNH_TEST(a_stick_direction_in_an_axis_is_the_positive_push) {
    auto s = slot({Source::stick_direction(AnalogStick::CirclePad, Direction::Left)});
    CHECK(near(eval_slot(ActionKind::Axis, s, view(0, {-0.3f, 0})).value, 0.3f));
    CHECK(eval_slot(ActionKind::Axis, s, view(0, {0.3f, 0})).value == 0.0f);
}

NNH_TEST(a_whole_stick_is_the_shaped_stick) {
    auto s = slot({Source::stick(AnalogStick::CirclePad)});
    const SlotOut out = eval_slot(ActionKind::Vector, s, view(0, {0.3f, -0.4f}));
    CHECK(near(out.vec.x, 0.3f) && near(out.vec.y, -0.4f) && near(out.value, 0.5f));
}

NNH_TEST(a_whole_stick_dead_zone_is_rescaled_to_start_at_zero_and_reach_one) {
    auto s = slot({.source = Source::stick(AnalogStick::CStick), .dead_zone = 0.2f});
    CHECK(eval_slot(ActionKind::Vector, s, view(0, {}, {0.2f, 0})).value == 0.0f);
    const SlotOut mid = eval_slot(ActionKind::Vector, s, view(0, {}, {0, 0.6f}));
    CHECK(near(mid.vec.y, 0.5f) && near(mid.vec.x, 0.0f));
    CHECK(near(eval_slot(ActionKind::Vector, s, view(0, {}, {-1.0f, 0})).vec.x, -1.0f));
}

NNH_TEST(a_stick_axis_reads_one_component_with_its_dead_zone) {
    auto x = slot({Source::stick_axis(AnalogStick::CirclePad, StickAxis::X)});
    auto y = slot({.source = Source::stick_axis(AnalogStick::CirclePad, StickAxis::Y), .dead_zone = 0.5f});
    CHECK(near(eval_slot(ActionKind::Axis, x, view(0, {-0.7f, 0.2f})).value, -0.7f));
    CHECK(eval_slot(ActionKind::Axis, y, view(0, {0, 0.5f})).value == 0.0f);
    CHECK(near(eval_slot(ActionKind::Axis, y, view(0, {0, -0.75f})).value, -0.5f));
}

NNH_TEST(a_two_button_composite_cancels_opposites) {
    auto s = slot({Source::buttons(Button::L, Button::R)});
    CHECK(eval_slot(ActionKind::Axis, s, view(bit(Button::L))).value == -1.0f);
    CHECK(eval_slot(ActionKind::Axis, s, view(bit(Button::R))).value == 1.0f);
    CHECK(eval_slot(ActionKind::Axis, s, view(bit(Button::L) | bit(Button::R))).value == 0.0f);
}

NNH_TEST(a_four_button_composite_has_length_one_on_a_diagonal) {
    auto s = slot({Source::dpad()});
    const SlotOut right = eval_slot(ActionKind::Vector, s, view(bit(Button::Right)));
    CHECK(right.vec.x == 1.0f && right.vec.y == 0.0f && right.value == 1.0f);
    const SlotOut diag = eval_slot(ActionKind::Vector, s, view(bit(Button::Up) | bit(Button::Left)));
    CHECK(diag.vec.x == -0.70710678f && diag.vec.y == 0.70710678f);
    CHECK(near(diag.value, 1.0f));
    const SlotOut both = eval_slot(ActionKind::Vector, s, view(bit(Button::Up) | bit(Button::Down)));
    CHECK(both.vec.x == 0.0f && both.vec.y == 0.0f && both.value == 0.0f);
}

NNH_TEST(an_empty_composite_part_reads_nothing) {
    Source half = Source::dpad();
    half.c = Source::kNoButton; // no Left
    auto s = slot({half});
    CHECK(eval_slot(ActionKind::Vector, s, view(bit(Button::Left))).vec.x == 0.0f);
}

NNH_TEST(a_touch_area_is_held_while_touched_inside_and_slides) {
    auto s = slot({Source::touch(10, 20, 30, 40)});
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {}, false, {15, 25})).held);
    CHECK(eval_slot(ActionKind::Button, s, view(0, {}, {}, true, {10, 20})).held);  // top-left edge inside
    CHECK(eval_slot(ActionKind::Button, s, view(0, {}, {}, true, {39, 59})).held);  // last pixel inside
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {}, true, {40, 30})).held); // slid out right
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {}, true, {20, 60})).held); // slid out below
    CHECK(!eval_slot(ActionKind::Button, s, view(0, {}, {}, true, {9, 30})).held);
}

NNH_TEST(invert_negates_x_and_y_after_everything_else) {
    auto ax = slot({.source = Source::stick_axis(AnalogStick::CirclePad, StickAxis::Y), .invert_x = true});
    CHECK(near(eval_slot(ActionKind::Axis, ax, view(0, {0, 0.5f})).value, -0.5f));
    auto vy = slot({.source = Source::dpad(), .invert_y = true});
    const SlotOut v = eval_slot(ActionKind::Vector, vy, view(bit(Button::Up) | bit(Button::Right)));
    CHECK(v.vec.x > 0.0f && v.vec.y < 0.0f);
    auto vx = slot({.source = Source::stick(AnalogStick::CirclePad), .invert_x = true});
    CHECK(near(eval_slot(ActionKind::Vector, vx, view(0, {0.5f, 0.5f})).vec.x, -0.5f));
}

NNH_TEST(slot_engaged_tells_whether_its_control_is_in_use) {
    using nnh::detail::slot_engaged;
    auto b = slot({Source::buttons(Button::L, Button::R)});
    CHECK(slot_engaged(b, view(bit(Button::R)), 0.5f));
    CHECK(!slot_engaged(b, view(bit(Button::A)), 0.5f));
    auto st = slot({Source::stick(AnalogStick::CirclePad)});
    CHECK(slot_engaged(st, view(0, {0.5f, 0}), 0.5f));
    CHECK(!slot_engaged(st, view(0, {0.45f, 0}), 0.5f));
    CHECK(slot_engaged(st, view(0, {0.45f, 0}), 0.4f));
    auto t = slot({Source::touch(0, 0, 10, 10)});
    CHECK(slot_engaged(t, view(0, {}, {}, true, {5, 5}), 0.5f));
    CHECK(!slot_engaged(t, view(0, {}, {}, true, {50, 5}), 0.5f));
}

NNH_TEST(sources_fit_only_their_action_kinds) {
    using nnh::detail::source_fits;
    using K = Source::Kind;
    CHECK(source_fits(ActionKind::Button, K::Button) && source_fits(ActionKind::Button, K::StickDirection));
    CHECK(source_fits(ActionKind::Button, K::Touch) && !source_fits(ActionKind::Button, K::Stick));
    CHECK(source_fits(ActionKind::Axis, K::StickAxis) && source_fits(ActionKind::Axis, K::Buttons2));
    CHECK(source_fits(ActionKind::Axis, K::StickDirection) && !source_fits(ActionKind::Axis, K::Button));
    CHECK(source_fits(ActionKind::Vector, K::Stick) && source_fits(ActionKind::Vector, K::Buttons4));
    CHECK(!source_fits(ActionKind::Vector, K::Touch) && !source_fits(ActionKind::Vector, K::Buttons2));
    CHECK(source_fits(ActionKind::Vector, K::None) && source_fits(ActionKind::Button, K::None));
}
