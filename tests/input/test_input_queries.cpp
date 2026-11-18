// Public input queries fed through the platform contract (spec 003: FR-1, FR-2, FR-4, FR-6, FR-12).

#include <cmath>
#include <initializer_list>

#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Button;
using nnh::Capability;
namespace input = nnh::input;

namespace {

void start(std::initializer_list<Capability> caps = {Capability::SecondScreen}) {
    nnh::shutdown();
    fake::reset();
    fake::set_capabilities(caps);
    CHECK(nnh::init({}));
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

} // namespace

NNH_TEST(sticks_read_zero_before_any_input) {
    start({Capability::CirclePad, Capability::CStick});
    CHECK(nnh::frame());
    CHECK(input::circle_pad().x == 0.0f && input::circle_pad().y == 0.0f);
    CHECK(input::c_stick().x == 0.0f && input::c_stick().y == 0.0f);
    nnh::shutdown();
}

NNH_TEST(circle_pad_reads_the_platform_value_with_the_dead_zone_applied) {
    start({Capability::CirclePad});
    fake::set_circle_pad(1.0f, 0.0f);
    CHECK(nnh::frame());
    CHECK(near(input::circle_pad().x, 1.0f) && input::circle_pad().y == 0.0f);
    fake::set_circle_pad(0.05f, -0.05f);
    CHECK(nnh::frame());
    CHECK(input::circle_pad().x == 0.0f && input::circle_pad().y == 0.0f);
    fake::set_circle_pad(-2.0f, 2.0f); // beyond the range: clamped to the unit circle
    CHECK(nnh::frame());
    const nnh::Stick s = input::circle_pad();
    CHECK(near(s.x * s.x + s.y * s.y, 1.0f) && s.x < 0 && s.y > 0);
    nnh::shutdown();
}

NNH_TEST(c_stick_reads_the_platform_value_when_the_system_has_one) {
    start({Capability::CirclePad, Capability::CStick});
    fake::set_c_stick(0.0f, -1.0f);
    CHECK(nnh::frame());
    CHECK(input::c_stick().x == 0.0f && near(input::c_stick().y, -1.0f));
    nnh::shutdown();
}

NNH_TEST(c_stick_reads_zero_when_the_system_has_none) {
    start({Capability::CirclePad});
    fake::set_c_stick(1.0f, 1.0f);
    CHECK(nnh::frame());
    CHECK(input::c_stick().x == 0.0f && input::c_stick().y == 0.0f);
    nnh::shutdown();
}

NNH_TEST(touch_queries_follow_a_touch_from_start_to_end) {
    start();
    CHECK(nnh::frame());
    CHECK(!input::touching() && !input::touch_started() && !input::touch_ended());
    fake::set_touch(100, 50);
    CHECK(nnh::frame());
    CHECK(input::touching() && input::touch_started() && !input::touch_ended());
    CHECK(input::touch_position().x == 100 && input::touch_position().y == 50);
    fake::set_touch(110, 60);
    CHECK(nnh::frame());
    CHECK(input::touching() && !input::touch_started());
    CHECK(input::touch_position().x == 110 && input::touch_position().y == 60);
    fake::release_touch();
    CHECK(nnh::frame());
    CHECK(!input::touching() && input::touch_ended());
    CHECK(input::touch_position().x == 110 && input::touch_position().y == 60); // last point kept
    CHECK(nnh::frame());
    CHECK(!input::touch_ended());
    nnh::shutdown();
}

NNH_TEST(input_state_is_forgotten_by_a_new_init) {
    start();
    fake::set_touch(5, 6);
    fake::set_circle_pad(1.0f, 0.0f);
    input::set_circle_pad_mirrors_dpad(true);
    CHECK(nnh::frame());
    start();
    CHECK(!input::touching() && input::touch_position().x == 0 && input::circle_pad().x == 0.0f);
    CHECK(!input::circle_pad_mirrors_dpad());
    nnh::shutdown();
}

NNH_TEST(circle_pad_does_not_hold_the_dpad_by_default) {
    start({Capability::CirclePad});
    CHECK(!input::circle_pad_mirrors_dpad());
    fake::set_circle_pad(1.0f, 0.0f);
    CHECK(nnh::frame());
    CHECK(!input::held(Button::Right));
    nnh::shutdown();
}

NNH_TEST(circle_pad_mirror_holds_presses_and_releases_the_dpad) {
    start({Capability::CirclePad});
    input::set_circle_pad_mirrors_dpad(true);
    CHECK(input::circle_pad_mirrors_dpad());
    fake::set_circle_pad(0.0f, 1.0f);
    CHECK(nnh::frame());
    CHECK(input::held(Button::Up) && input::pressed(Button::Up));
    CHECK((input::held_mask() & static_cast<std::uint32_t>(Button::Up)) != 0);
    CHECK(nnh::frame());
    CHECK(input::held(Button::Up) && !input::pressed(Button::Up));
    fake::set_circle_pad(0.0f, 0.0f);
    CHECK(nnh::frame());
    CHECK(!input::held(Button::Up) && input::released(Button::Up));
    nnh::shutdown();
}

NNH_TEST(circle_pad_mirror_publishes_button_events) {
    start({Capability::CirclePad});
    int presses = 0;
    nnh::Subscription sub = nnh::events::subscribe<nnh::ButtonPressed>([&presses](const nnh::ButtonPressed& e) {
        if (e.button == Button::Left) ++presses;
    });
    input::set_circle_pad_mirrors_dpad(true);
    fake::set_circle_pad(-1.0f, 0.0f);
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(presses == 1);
    nnh::shutdown();
}

NNH_TEST(circle_pad_mirror_combines_with_the_real_dpad) {
    start({Capability::CirclePad});
    input::set_circle_pad_mirrors_dpad(true);
    fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
    fake::set_circle_pad(1.0f, 0.0f);
    CHECK(nnh::frame());
    CHECK(input::held(Button::Down) && input::held(Button::Right));
    fake::set_circle_pad(0.0f, 0.0f); // the real D-Pad stays held
    CHECK(nnh::frame());
    CHECK(input::held(Button::Down) && input::released(Button::Right));
    nnh::shutdown();
}

NNH_TEST(turning_the_mirror_off_releases_its_directions_on_the_next_frame) {
    start({Capability::CirclePad});
    input::set_circle_pad_mirrors_dpad(true);
    fake::set_circle_pad(1.0f, 0.0f);
    CHECK(nnh::frame());
    input::set_circle_pad_mirrors_dpad(false);
    CHECK(input::held(Button::Right)); // this frame's answer does not change
    CHECK(nnh::frame());
    CHECK(!input::held(Button::Right) && input::released(Button::Right));
    nnh::shutdown();
}
