// Systems without a control (spec 017 T2.6, FR-9): fallback slots, inactive ZL/ZR/C-Stick/touch sources,
// usable(), one Development warning when a context with an unusable action is enabled.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Capability;
using nnh::Source;
using nnh::StickAxis;

namespace {
enum class Act { Aim, Dash, Only };

ActionSet make() {
    ActionSet set{{.actions = 3}};
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)},
                                          {.source = Source::buttons(Button::L, Button::R),
                                           .fallback = true,
                                           .fallback_for = Capability::CStick}}}));
    CHECK(set.add(Act::Dash, {.name = "dash", .bindings = {btn(Button::ZL), btn(Button::Y)}}));
    CHECK(set.add(Act::Only, {.name = "only", .bindings = {btn(Button::ZR)}}));
    step(set, 0);
    return set;
}
} // namespace

NNH_TEST(on_an_old_3ds_the_fallback_slot_is_used) {
    restart(/*old_3ds=*/true);
    ActionSet set = make();
    step(set, bit(Button::R), {}, {-1.0f, 0});
    CHECK(set.value(Act::Aim) == 1.0f);
    CHECK(set.usable(Act::Aim));
    nnh::shutdown();
}

NNH_TEST(on_a_new_3ds_the_fallback_slot_is_ignored) {
    restart();
    ActionSet set = make();
    step(set, bit(Button::R));
    CHECK(set.value(Act::Aim) == 0.0f);
    step(set, 0, {}, {-1.0f, 0});
    CHECK(set.value(Act::Aim) == -1.0f);
    nnh::shutdown();
}

NNH_TEST(zl_and_zr_slots_are_inactive_without_z_buttons) {
    restart(/*old_3ds=*/true);
    ActionSet set = make();
    step(set, bit(Button::ZL) | bit(Button::ZR));
    CHECK(!set.held(Act::Dash) && !set.held(Act::Only));
    CHECK(set.usable(Act::Dash) && !set.usable(Act::Only));
    step(set, bit(Button::Y));
    CHECK(set.pressed(Act::Dash));
    nnh::shutdown();
}

NNH_TEST(every_slot_is_usable_on_a_new_3ds) {
    restart();
    ActionSet set = make();
    CHECK(set.usable(Act::Aim) && set.usable(Act::Dash) && set.usable(Act::Only));
    step(set, bit(Button::ZR));
    CHECK(set.pressed(Act::Only));
    nnh::shutdown();
}

NNH_TEST(touch_and_circle_pad_slots_need_their_capabilities) {
    nnh::shutdown();
    nnh::platform_fake::reset(); // SecondScreen only
    CHECK(nnh::init({}));
    ActionSet set{{.actions = 2}};
    CHECK(set.add(Act::Aim, {.name = "aim", .bindings = {{Source::touch(0, 0, 10, 10)}}}));
    CHECK(set.add(Act::Dash, {.name = "dash",
                              .bindings = {{Source::stick_direction(AnalogStick::CirclePad, nnh::Direction::Up)}}}));
    CHECK(!set.usable(Act::Aim) && !set.usable(Act::Dash));
    nnh::shutdown();
}

NNH_TEST(enabling_a_context_with_an_unusable_action_warns_once_in_development) {
    restart(/*old_3ds=*/true);
    const int lines = nnh::platform_fake::state().console_lines;
    ActionSet set = make(); // the first evaluation enables the default context
    if (nnh::build::kDevToolsEnabled) {
        CHECK(nnh::platform_fake::state().console_lines == lines + 1);
        CHECK(last_log_mentions("only") && last_log_mentions("default"));
    } else {
        CHECK(nnh::platform_fake::state().console_lines == lines);
    }
    set.enable(nnh::ContextId(0), false);
    step(set, 0);
    set.enable(nnh::ContextId(0), true);
    step(set, 0);
    CHECK(nnh::platform_fake::state().console_lines == lines + (nnh::build::kDevToolsEnabled ? 1 : 0));
    nnh::shutdown();
}
