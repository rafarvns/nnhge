// Listening for the next control (spec 017 T4.1, FR-16, AC-4): held-at-start ignored, capture order and rules per
// kind, cancel, exclude, sticks off, timeout, missing controls, everything idle while listening, blocking after.

#include "actions_fixture.hpp"
#include "nnh/events.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Direction;
using nnh::ListenState;
using nnh::Part;
using nnh::Source;
using nnh::StickAxis;

namespace {
enum class Act { Jump, Aim, Move, Pause };

ActionSet* g_set = nullptr;
void listen_locked() { g_set->listen(Act::Pause, 0); }
void listen_bad_part() { g_set->listen(Act::Jump, 0, {.part = Part::Up}); }
void listen_bad_slot() { g_set->listen(Act::Jump, 4); }

ActionSet make() {
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)},
                                          {Source::buttons(Button::L, Button::R)}}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}));
    frame();
    return set;
}

const nnh::RemapResult& result(const ActionSet& s) { return s.listen_result(); }
} // namespace

NNH_TEST(a_listen_captures_the_next_button_into_the_slot) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 1);
    CHECK(set.listening() && result(set).state == ListenState::Listening);
    frame();
    frame({.buttons = bit(Button::X)});
    CHECK(!set.listening() && result(set).state == ListenState::Bound);
    CHECK(result(set).source == Source::button(Button::X) && result(set).slot == 1);
    CHECK(set.source(Act::Jump, 1) == Source::button(Button::X));
    nnh::shutdown();
}

NNH_TEST(a_control_held_when_listening_starts_is_ignored_until_released) {
    restart();
    ActionSet set = make();
    frame({.buttons = bit(Button::A)}); // the A that chose "Jump" in a menu
    set.listen(Act::Jump, 0);
    frame({.buttons = bit(Button::A)});
    CHECK(set.listening());
    frame();
    frame({.buttons = bit(Button::A)}); // pressed again: captured
    CHECK(result(set).state == ListenState::Bound && result(set).source == Source::button(Button::A));
    nnh::shutdown();
}

NNH_TEST(buttons_are_captured_in_bit_order_before_sticks) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0);
    frame({.buttons = bit(Button::Y) | bit(Button::B), .cx = 1.0f});
    CHECK(result(set).source == Source::button(Button::B));
    nnh::shutdown();
}

NNH_TEST(a_button_action_captures_a_stick_direction) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 1);
    frame({.sx = 0.0f, .sy = -1.0f});
    CHECK(result(set).source == Source::stick_direction(AnalogStick::CStick, Direction::Down));
    set.listen(Act::Jump, 2);
    frame();
    frame({.cx = -0.4f, .sx = 1.0f}); // Circle Pad under 0.5: the C-Stick wins
    CHECK(result(set).source == Source::stick_direction(AnalogStick::CStick, Direction::Right));
    nnh::shutdown();
}

NNH_TEST(an_axis_captures_the_stick_axis_and_inverts_on_the_negative_side) {
    restart();
    ActionSet set = make();
    set.listen(Act::Aim, 0);
    frame({.buttons = bit(Button::X)}); // buttons don't bind a whole axis
    CHECK(set.listening());
    frame({.cy = -1.0f});
    CHECK(result(set).source == Source::stick_axis(AnalogStick::CirclePad, StickAxis::Y));
    frame();
    frame({.cy = 1.0f});
    CHECK(set.value(Act::Aim) == -1.0f); // inverted: down was "positive"
    set.listen(Act::Aim, 0);
    frame({.sx = 1.0f});
    CHECK(result(set).source == Source::stick_axis(AnalogStick::CStick, StickAxis::X));
    frame();
    frame({.sx = 1.0f});
    CHECK(set.value(Act::Aim) == 1.0f);
    nnh::shutdown();
}

NNH_TEST(a_vector_captures_the_whole_stick) {
    restart();
    ActionSet set = make();
    set.listen(Act::Move, 0);
    frame({.buttons = bit(Button::Up)});
    CHECK(set.listening());
    frame({.sx = -1.0f});
    CHECK(result(set).source == Source::stick(AnalogStick::CStick));
    nnh::shutdown();
}

NNH_TEST(a_composite_part_captures_a_button_only) {
    restart();
    ActionSet set = make();
    set.listen(Act::Move, 1, {.part = Part::Left});
    frame({.cx = 1.0f});
    CHECK(set.listening());
    frame({.buttons = bit(Button::Y)});
    CHECK(result(set).part == Part::Left);
    CHECK(set.source(Act::Move, 1) == Source::buttons(Button::Up, Button::Down, Button::Y, Button::Right));
    nnh::shutdown();
}

NNH_TEST(listening_a_part_of_a_simple_slot_makes_an_empty_composite) {
    restart();
    ActionSet set = make();
    set.listen(Act::Aim, 0, {.part = Part::Positive});
    frame({.buttons = bit(Button::ZR)});
    Source expected = Source::buttons(Button::ZL, Button::ZR);
    expected.a = Source::kNoButton;
    CHECK(set.source(Act::Aim, 0) == expected);
    nnh::shutdown();
}

NNH_TEST(the_cancel_control_cancels_and_changes_nothing) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0);
    frame({.buttons = bit(Button::Start) | bit(Button::B)});
    CHECK(!set.listening() && result(set).state == ListenState::Cancelled);
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::A));
    set.listen(Act::Jump, 0, {.on_conflict = nnh::ConflictPolicy::Allow, .cancel = bit(Button::Select)});
    frame();
    frame({.buttons = bit(Button::Start)}); // not a cancel any more: captured (Pause keeps it too)
    CHECK(result(set).state == ListenState::Bound && result(set).source == Source::button(Button::Start));
    nnh::shutdown();
}

NNH_TEST(excluded_controls_and_sticks_off_are_never_captured) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0, {.exclude = bit(Button::Select), .sticks = false});
    frame({.buttons = bit(Button::Select), .cx = 1.0f});
    CHECK(set.listening());
    frame({.buttons = bit(Button::Select) | bit(Button::L), .cx = 1.0f});
    CHECK(result(set).source == Source::button(Button::L));
    nnh::shutdown();
}

NNH_TEST(a_timeout_ends_the_listen) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0, {.timeout_frames = 3});
    frames(3);
    CHECK(set.listening());
    frame();
    CHECK(!set.listening() && result(set).state == ListenState::TimedOut);
    nnh::shutdown();
}

NNH_TEST(controls_the_system_lacks_are_not_captured) {
    restart(/*old_3ds=*/true);
    ActionSet set = make();
    set.listen(Act::Jump, 0);
    frame({.buttons = bit(Button::ZL), .sx = 1.0f});
    CHECK(set.listening());
    frame({.buttons = bit(Button::ZL) | bit(Button::X)});
    CHECK(result(set).source == Source::button(Button::X));
    nnh::shutdown();
}

NNH_TEST(every_action_reads_idle_and_silent_while_listening) {
    restart();
    ActionSet set = make();
    int events = 0;
    nnh::Subscription a = nnh::events::subscribe<nnh::ActionPressed>([&](const nnh::ActionPressed&) { ++events; });
    nnh::Subscription b = nnh::events::subscribe<nnh::ActionReleased>([&](const nnh::ActionReleased&) { ++events; });
    frame({.cx = 1.0f});
    CHECK(set.held(Act::Move) && events == 1);
    set.listen(Act::Jump, 0, {.sticks = false});
    frame({.buttons = bit(Button::Start) | bit(Button::L), .cx = 1.0f}); // Start cancels; L... cancel first
    CHECK(!set.held(Act::Move) && !set.released(Act::Move) && events == 1);
    nnh::shutdown();
}

NNH_TEST(the_control_that_binds_does_not_also_act) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0);
    frame({.buttons = bit(Button::B)});
    CHECK(!set.pressed(Act::Jump));
    frame({.buttons = bit(Button::B)}); // after the listen: blocked until released
    CHECK(!set.pressed(Act::Jump) && !set.held(Act::Jump));
    frame();
    frame({.buttons = bit(Button::B)});
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(stop_listening_cancels_and_the_result_stays_until_cleared) {
    restart();
    ActionSet set = make();
    set.listen(Act::Jump, 0);
    set.stop_listening();
    CHECK(!set.listening() && result(set).state == ListenState::Cancelled);
    frames(3);
    CHECK(result(set).state == ListenState::Cancelled);
    set.clear_listen_result();
    CHECK(result(set).state == ListenState::Idle);
    nnh::shutdown();
}

NNH_TEST(listening_a_locked_or_invalid_slot_asserts_or_is_ignored) {
    restart();
    ActionSet set = make();
    g_set = &set;
    CHECK(asserts_or_is_ignored(listen_locked));
    CHECK(asserts_or_is_ignored(listen_bad_part));
    CHECK(asserts_or_is_ignored(listen_bad_slot));
    CHECK(!set.listening());
    nnh::shutdown();
}

NNH_TEST(an_empty_set_never_listens) {
    ActionSet set;
    set.listen(Act::Jump, 0);
    CHECK(!set.listening() && set.listen_result().state == ListenState::Idle);
}
