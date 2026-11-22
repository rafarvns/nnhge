// Conflicts (spec 017 T4.2, FR-17, AC-4): a source already used in the same context is swapped, replaced, allowed
// or rejected; locked bindings win; other contexts never conflict; conflicts() lists shared controls.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::ConflictPolicy;
using nnh::Direction;
using nnh::ListenState;
using nnh::Part;
using nnh::Resolution;
using nnh::Source;

namespace {
enum class Act { Jump, Attack, Pause, Confirm, Move, Dash };
enum class Ctx { Play, Menu };

ActionSet make() {
    ActionSet set{{.actions = 6, .contexts = 2}};
    CHECK(set.add_context(Ctx::Menu, "menu"));
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Attack, {.name = "attack", .bindings = {btn(Button::B), btn(Button::Y)}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}));
    CHECK(set.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu, .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.add(Act::Dash, {.name = "dash", .bindings = {{Source::stick_direction(AnalogStick::CStick, Direction::Up)}}}));
    return set;
}
} // namespace

NNH_TEST(swap_gives_the_other_binding_this_ones_previous_source) {
    restart();
    ActionSet set = make();
    const nnh::RemapResult r = set.set_source(Act::Jump, 0, Source::button(Button::B));
    CHECK(r.state == ListenState::Bound && r.resolution == Resolution::Swapped);
    CHECK(r.other_action == static_cast<int>(Act::Attack) && r.other_slot == 0 && r.conflicts == 1);
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::B));
    CHECK(set.source(Act::Attack, 0) == Source::button(Button::A));
    CHECK(set.source(Act::Confirm, 0) == Source::button(Button::A)); // other context untouched
    nnh::shutdown();
}

NNH_TEST(swap_clears_the_other_binding_when_the_previous_source_doesnt_fit) {
    restart();
    ActionSet set = make();
    // Dash (button action, C-Stick Up) takes the whole C-Stick's place? No: Move takes the C-Stick; its previous
    // source (the whole Circle Pad) can't go into Dash, so Dash is cleared.
    const nnh::RemapResult r = set.set_source(Act::Move, 0, Source::stick(AnalogStick::CStick));
    CHECK(r.resolution == Resolution::Replaced && r.other_action == static_cast<int>(Act::Dash));
    CHECK(set.source(Act::Dash, 0).kind == Source::Kind::None);
    nnh::shutdown();
}

NNH_TEST(swap_gives_a_part_the_previous_button) {
    restart();
    ActionSet set = make();
    const nnh::RemapResult r = set.set_part(Act::Move, 1, Part::Up, Button::B);
    CHECK(r.resolution == Resolution::Swapped && r.other_action == static_cast<int>(Act::Attack));
    CHECK(set.source(Act::Attack, 0) == Source::button(Button::Up));
    CHECK(set.source(Act::Move, 1) == Source::buttons(Button::B, Button::Down, Button::Left, Button::Right));
    nnh::shutdown();
}

NNH_TEST(swap_inside_one_composite_exchanges_its_parts) {
    restart();
    ActionSet set = make();
    const nnh::RemapResult r = set.set_part(Act::Move, 1, Part::Down, Button::Up);
    CHECK(r.resolution == Resolution::Swapped && r.other_part == Part::Up);
    CHECK(set.source(Act::Move, 1) == Source::buttons(Button::Down, Button::Up, Button::Left, Button::Right));
    nnh::shutdown();
}

NNH_TEST(replace_clears_every_other_binding_of_the_source) {
    restart();
    ActionSet set = make();
    CHECK(set.set_source(Act::Attack, 1, Source::button(Button::A), ConflictPolicy::Allow).resolution ==
          Resolution::Allowed);
    const nnh::RemapResult r = set.set_source(Act::Move, 1, Source::buttons(Button::A, Button::X, Button::L, Button::R),
                                              ConflictPolicy::Replace);
    CHECK(r.resolution == Resolution::Replaced && r.conflicts == 2);
    CHECK(set.source(Act::Jump, 0).kind == Source::Kind::None && set.source(Act::Attack, 1).kind == Source::Kind::None);
    nnh::shutdown();
}

NNH_TEST(allow_keeps_both_bindings) {
    restart();
    ActionSet set = make();
    const nnh::RemapResult r = set.set_source(Act::Jump, 0, Source::button(Button::B), ConflictPolicy::Allow);
    CHECK(r.state == ListenState::Bound && r.resolution == Resolution::Allowed && r.conflicts == 1);
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::B) && set.source(Act::Attack, 0) == Source::button(Button::B));
    nnh::shutdown();
}

NNH_TEST(reject_changes_nothing_and_names_the_holder) {
    restart();
    ActionSet set = make();
    const nnh::RemapResult r = set.set_source(Act::Jump, 0, Source::button(Button::Y), ConflictPolicy::Reject);
    CHECK(r.state == ListenState::Rejected && r.resolution == Resolution::Rejected);
    CHECK(r.other_action == static_cast<int>(Act::Attack) && r.other_slot == 1);
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::A));
    nnh::shutdown();
}

NNH_TEST(a_locked_binding_rejects_under_every_policy_but_allow) {
    restart();
    ActionSet set = make();
    for (ConflictPolicy p : {ConflictPolicy::Swap, ConflictPolicy::Replace, ConflictPolicy::Reject}) {
        const nnh::RemapResult r = set.set_source(Act::Jump, 0, Source::button(Button::Start), p);
        CHECK(r.state == ListenState::Rejected && r.other_action == static_cast<int>(Act::Pause));
        CHECK(set.source(Act::Jump, 0) == Source::button(Button::A));
    }
    CHECK(set.set_source(Act::Jump, 0, Source::button(Button::Start), ConflictPolicy::Allow).state ==
          ListenState::Bound);
    CHECK(set.source(Act::Pause, 0) == Source::button(Button::Start));
    nnh::shutdown();
}

NNH_TEST(a_listen_resolves_conflicts_with_its_policy) {
    restart();
    ActionSet set = make();
    frame();
    set.listen(Act::Jump, 0, {.on_conflict = ConflictPolicy::Reject});
    frame({.buttons = bit(Button::B)});
    CHECK(set.listen_result().state == ListenState::Rejected && set.source(Act::Jump, 0) == Source::button(Button::A));
    set.listen(Act::Jump, 0);
    frame();
    frame({.buttons = bit(Button::B)});
    CHECK(set.listen_result().resolution == Resolution::Swapped);
    CHECK(set.source(Act::Attack, 0) == Source::button(Button::A));
    nnh::shutdown();
}

NNH_TEST(sticks_conflict_by_direction) {
    restart();
    ActionSet set = make();
    // The whole C-Stick covers Dash's C-Stick Up; the Circle Pad's Up doesn't.
    CHECK(set.set_source(Act::Dash, 0, Source::stick_direction(AnalogStick::CirclePad, Direction::Up),
                         ConflictPolicy::Reject)
              .other_action == static_cast<int>(Act::Move));
    CHECK(set.set_source(Act::Jump, 0, Source::stick_direction(AnalogStick::CStick, Direction::Down),
                         ConflictPolicy::Reject)
              .conflicts == 0);
    nnh::shutdown();
}

NNH_TEST(conflicts_lists_every_binding_sharing_a_control) {
    restart();
    ActionSet set = make();
    nnh::Conflict out[4];
    CHECK(set.conflicts(Act::Jump, out, 4) == 0);
    set.set_source(Act::Jump, 0, Source::button(Button::Y), ConflictPolicy::Allow);
    set.set_part(Act::Move, 1, Part::Left, Button::Y, ConflictPolicy::Allow);
    CHECK(set.conflicts(Act::Jump, out, 4) == 2);
    CHECK(out[0].slot == 0 && out[0].part == Part::Whole && out[0].other_action == static_cast<int>(Act::Attack));
    CHECK(out[0].other_slot == 1 && out[1].other_action == static_cast<int>(Act::Move) && out[1].other_part == Part::Left);
    CHECK(set.conflicts(Act::Jump, out, 1) == 2); // the count, even when only one fits
    CHECK(set.conflicts(Act::Move, out, 4) == 2);
    CHECK(out[0].slot == 1 && out[0].part == Part::Left);
    nnh::shutdown();
}
