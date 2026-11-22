// Display names of controls and sources (spec 017 T1.4, FR-19).

#include "actions_fixture.hpp"

#include <cstring>
#include <string_view>

using namespace actions_test;
using nnh::AnalogStick;
using nnh::Direction;
using nnh::Source;
using nnh::StickAxis;
using nnh::actions::control_name;

namespace {
bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

std::string_view described(Source s, std::size_t capacity = 64) {
    static char out[64];
    nnh::actions::describe(s, out, capacity);
    return out;
}
} // namespace

NNH_TEST(every_button_has_a_short_name) {
    const char* expected[] = {"A",     "B",      "X",         "Y",           "L",           "R",          "ZL",
                              "ZR",    "Start",  "Select",    "D-Pad Up",    "D-Pad Down",  "D-Pad Left", "D-Pad Right"};
    for (int i = 0; i < 14; ++i) {
        CHECK(is(control_name(Source::button(static_cast<Button>(1u << i))), expected[i]));
    }
}

NNH_TEST(stick_sources_are_named_by_stick_and_direction_or_axis) {
    CHECK(is(control_name(Source::stick_direction(AnalogStick::CirclePad, Direction::Left)), "Circle Pad Left"));
    CHECK(is(control_name(Source::stick_direction(AnalogStick::CStick, Direction::Up)), "C-Stick Up"));
    CHECK(is(control_name(Source::stick_direction(AnalogStick::CirclePad, Direction::Down)), "Circle Pad Down"));
    CHECK(is(control_name(Source::stick_direction(AnalogStick::CStick, Direction::Right)), "C-Stick Right"));
    CHECK(is(control_name(Source::stick(AnalogStick::CStick)), "C-Stick"));
    CHECK(is(control_name(Source::stick(AnalogStick::CirclePad)), "Circle Pad"));
    CHECK(is(control_name(Source::stick_axis(AnalogStick::CStick, StickAxis::X)), "C-Stick X"));
    CHECK(is(control_name(Source::stick_axis(AnalogStick::CirclePad, StickAxis::Y)), "Circle Pad Y"));
}

NNH_TEST(touch_and_none_have_names) {
    CHECK(is(control_name(Source::touch(0, 0, 10, 10)), "Touch"));
    CHECK(is(control_name(Source{}), ""));
}

NNH_TEST(composites_join_their_parts_with_a_slash) {
    CHECK(described(Source::buttons(Button::L, Button::R)) == "L/R");
    CHECK(described(Source::buttons(Button::X, Button::B, Button::Y, Button::A)) == "X/B/Y/A");
    CHECK(described(Source::buttons(Button::Left, Button::Right)) == "D-Pad Left/D-Pad Right");
}

NNH_TEST(the_d_pad_composite_is_called_d_pad) {
    CHECK(described(Source::dpad()) == "D-Pad");
    CHECK(described(Source::buttons(Button::Down, Button::Up, Button::Left, Button::Right)) != "D-Pad");
}

NNH_TEST(an_empty_composite_part_shows_a_dash) {
    Source s = Source::buttons(Button::L, Button::R);
    s.a = Source::kNoButton;
    CHECK(described(s) == "-/R");
}

NNH_TEST(single_sources_are_described_by_their_name) {
    CHECK(described(Source::button(Button::ZR)) == "ZR");
    CHECK(described(Source{}).empty());
}

NNH_TEST(describe_truncates_and_ends_with_nul) {
    char out[4];
    const std::size_t n = nnh::actions::describe(Source::stick(AnalogStick::CirclePad), out, sizeof out);
    CHECK(n == 3 && std::strcmp(out, "Cir") == 0);
    CHECK(nnh::actions::describe(Source::stick(AnalogStick::CirclePad), out, 0) == 0);
}
