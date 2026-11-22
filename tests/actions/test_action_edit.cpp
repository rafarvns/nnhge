// Editing and resetting bindings (spec 017 T4.3, FR-8, FR-18, FR-19): set, clear, invert, reset of a slot, an
// action, a context and the set; locked slots never change; effects from the next evaluation; describe().

#include "actions_fixture.hpp"

#include <cstring>
#include <string_view>

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Source;
using nnh::StickAxis;

namespace {
enum class Act { Jump, Aim, Pause, Confirm };
enum class Ctx { Play, Menu };

ActionSet* g_set = nullptr;
void set_locked() { g_set->set_source(Act::Pause, 0, Source::button(Button::X)); }
void clear_locked() { g_set->clear(Act::Pause, 0); }
void set_touch() { g_set->set_source(Act::Jump, 1, Source::touch(0, 0, 9, 9)); }
void set_unfit() { g_set->set_source(Act::Jump, 1, Source::stick(AnalogStick::CirclePad)); }

ActionSet make() {
    ActionSet set{{.actions = 4, .contexts = 2}};
    CHECK(set.add_context(Ctx::Menu, "menu"));
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::B)}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)}}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}));
    CHECK(set.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu, .bindings = {btn(Button::X)}}));
    frame();
    return set;
}

std::string_view described(const ActionSet& set, Act a) {
    static char out[64];
    set.describe(a, out, sizeof out);
    return out;
}
} // namespace

NNH_TEST(a_new_source_applies_from_the_next_evaluation) {
    restart();
    ActionSet set = make();
    frame({.buttons = bit(Button::A)});
    set.set_source(Act::Jump, 0, Source::button(Button::X));
    CHECK(set.held(Act::Jump)); // this frame's values stay
    frame({.buttons = bit(Button::A)});
    CHECK(!set.held(Act::Jump));
    frame({.buttons = bit(Button::X)});
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(clear_empties_a_slot_and_reset_restores_its_default) {
    restart();
    ActionSet set = make();
    set.clear(Act::Jump, 1);
    CHECK(set.source(Act::Jump, 1).kind == Source::Kind::None);
    frame({.buttons = bit(Button::B)});
    CHECK(!set.held(Act::Jump));
    set.reset(Act::Jump, 1);
    CHECK(set.source(Act::Jump, 1) == Source::button(Button::B));
    nnh::shutdown();
}

NNH_TEST(set_invert_flips_an_axis_and_reset_restores_it) {
    restart();
    ActionSet set = make();
    set.set_invert(Act::Aim, 0, true, false);
    frame({.sx = 1.0f});
    CHECK(set.value(Act::Aim) == -1.0f);
    set.reset(Act::Aim);
    frame({.sx = 1.0f});
    CHECK(set.value(Act::Aim) == 1.0f);
    nnh::shutdown();
}

NNH_TEST(resets_cover_an_action_a_context_and_the_whole_set) {
    restart();
    ActionSet set = make();
    set.set_source(Act::Jump, 0, Source::button(Button::Y));
    set.set_source(Act::Jump, 1, Source::button(Button::L));
    set.set_source(Act::Confirm, 0, Source::button(Button::R));
    set.reset(Act::Jump);
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::A) && set.source(Act::Jump, 1) == Source::button(Button::B));
    CHECK(set.source(Act::Confirm, 0) == Source::button(Button::R));
    set.set_source(Act::Jump, 0, Source::button(Button::Y));
    set.reset_context(Ctx::Menu);
    CHECK(set.source(Act::Confirm, 0) == Source::button(Button::X) && set.source(Act::Jump, 0) == Source::button(Button::Y));
    set.set_source(Act::Confirm, 0, Source::button(Button::R));
    set.reset_all();
    CHECK(set.source(Act::Jump, 0) == Source::button(Button::A) && set.source(Act::Confirm, 0) == Source::button(Button::X));
    nnh::shutdown();
}

NNH_TEST(locked_slots_never_change) {
    restart();
    ActionSet set = make();
    g_set = &set;
    CHECK(asserts_or_is_ignored(set_locked));
    CHECK(asserts_or_is_ignored(clear_locked));
    set.reset_all();
    CHECK(set.source(Act::Pause, 0) == Source::button(Button::Start));
    nnh::shutdown();
}

NNH_TEST(touch_and_unfit_sources_cant_be_set) {
    restart();
    ActionSet set = make();
    g_set = &set;
    CHECK(asserts_or_is_ignored(set_touch));
    CHECK(asserts_or_is_ignored(set_unfit));
    CHECK(set.source(Act::Jump, 1) == Source::button(Button::B));
    nnh::shutdown();
}

NNH_TEST(describe_lists_the_active_slots_of_an_action) {
    restart(/*old_3ds=*/true);
    ActionSet set = make();
    CHECK(described(set, Act::Jump) == "A / B");
    CHECK(described(set, Act::Aim).empty()); // no C-Stick
    set.clear(Act::Jump, 0);
    CHECK(described(set, Act::Jump) == "B");
    char small[3];
    set.set_source(Act::Jump, 0, Source::button(Button::Select));
    CHECK(set.describe(Act::Jump, small, sizeof small) == 2 && std::strcmp(small, "Se") == 0);
    nnh::shutdown();
}
