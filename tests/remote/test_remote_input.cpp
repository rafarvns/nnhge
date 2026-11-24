// Console remote input (spec 013 FR-1): apply messages, merge with the real input, release all.

#include <cstdint>

#include "nnh/input.hpp"
#include "remote/remote_input.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;
using nnh::platform::InputSample;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t kB = static_cast<std::uint32_t>(nnh::Button::B);
constexpr std::uint32_t kStart = static_cast<std::uint32_t>(nnh::Button::Start);
constexpr std::uint32_t kRight = static_cast<std::uint32_t>(nnh::Button::Right);

Message buttons(std::uint32_t press, std::uint32_t release) {
    Message m;
    m.type = MessageType::Buttons;
    m.press = press;
    m.release = release;
    return m;
}

Message stick(std::uint8_t which, std::int16_t x, std::int16_t y) {
    Message m;
    m.type = MessageType::Stick;
    m.stick = which;
    m.x = x;
    m.y = y;
    return m;
}

Message touch(bool down, std::int16_t x, std::int16_t y) {
    Message m;
    m.type = MessageType::Touch;
    m.down = down;
    m.x = x;
    m.y = y;
    return m;
}

InputSample merged(const RemoteInput& remote, InputSample real = {}) {
    remote.merge_into(real);
    return real;
}

} // namespace

NNH_TEST(remote_input_press_holds_and_release_lets_go) {
    RemoteInput r;
    r.apply(buttons(kA | kStart, 0));
    CHECK(r.buttons == (kA | kStart));
    r.apply(buttons(kB, kA));
    CHECK(r.buttons == (kB | kStart));
    r.apply(buttons(0, kRight)); // not held: nothing else changes
    CHECK(r.buttons == (kB | kStart));
}

NNH_TEST(remote_input_buttons_are_or_ed_with_the_real_ones) {
    RemoteInput r;
    r.apply(buttons(kA, 0));
    InputSample real;
    real.buttons = kB;
    CHECK(merged(r, real).buttons == (kA | kB));
    real.buttons = kA; // both hold A: still just A
    CHECK(merged(r, real).buttons == kA);
}

NNH_TEST(remote_input_a_remote_release_does_not_hide_a_real_press) {
    RemoteInput r;
    r.apply(buttons(kA, 0));
    r.apply(buttons(0, kA));
    InputSample real;
    real.buttons = kA;
    CHECK(merged(r, real).buttons == kA);
}

NNH_TEST(remote_input_the_stick_pushed_further_wins) {
    RemoteInput r;
    r.apply(stick(0, 500, -250));
    const InputSample out = merged(r);
    CHECK(out.circle_x == 0.5f && out.circle_y == -0.25f);
    InputSample real;
    real.circle_x = 0.0f;
    real.circle_y = 0.75f; // further than (0.5, -0.25)
    const InputSample kept = merged(r, real);
    CHECK(kept.circle_x == 0.0f && kept.circle_y == 0.75f);
}

NNH_TEST(remote_input_a_slightly_further_remote_stick_wins) {
    RemoteInput r;
    r.apply(stick(0, 0, 501));
    InputSample real;
    real.circle_x = 0.5f;
    const InputSample out = merged(r, real);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.501f);
}

NNH_TEST(remote_input_equal_sticks_keep_the_real_one) {
    RemoteInput r;
    r.apply(stick(0, 0, 500));
    InputSample real;
    real.circle_x = 0.5f;
    const InputSample out = merged(r, real);
    CHECK(out.circle_x == 0.5f && out.circle_y == 0.0f);
}

NNH_TEST(remote_input_a_centred_remote_stick_keeps_the_real_one) {
    RemoteInput r;
    InputSample real;
    real.circle_x = 0.01f;
    real.cstick_y = -0.02f;
    const InputSample out = merged(r, real);
    CHECK(out.circle_x == 0.01f && out.cstick_y == -0.02f);
}

NNH_TEST(remote_input_the_c_stick_is_separate) {
    RemoteInput r;
    r.apply(stick(1, -1000, 1000));
    const InputSample out = merged(r);
    CHECK(out.cstick_x == -1.0f && out.cstick_y == 1.0f);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.0f);
    r.apply(stick(1, 0, 0));
    CHECK(merged(r).cstick_x == 0.0f);
}

NNH_TEST(remote_input_a_remote_touch_wins_while_down) {
    RemoteInput r;
    r.apply(touch(true, 40, 200));
    InputSample real;
    real.touching = true;
    real.touch_x = 300;
    real.touch_y = 10;
    InputSample out = merged(r, real);
    CHECK(out.touching && out.touch_x == 40 && out.touch_y == 200);
    out = merged(r);
    CHECK(out.touching && out.touch_x == 40 && out.touch_y == 200);
}

NNH_TEST(remote_input_a_lifted_remote_touch_keeps_the_real_one) {
    RemoteInput r;
    r.apply(touch(true, 40, 200));
    r.apply(touch(false, 0, 0));
    InputSample real;
    real.touching = true;
    real.touch_x = 300;
    real.touch_y = 10;
    InputSample out = merged(r, real);
    CHECK(out.touching && out.touch_x == 300 && out.touch_y == 10);
    CHECK(!merged(r).touching);
}

NNH_TEST(remote_input_ignores_messages_that_are_not_input) {
    RemoteInput r;
    r.apply(buttons(kA, 0));
    Message other;
    other.type = MessageType::Screenshot;
    other.screens = 3;
    r.apply(other);
    other.type = MessageType::Bye;
    r.apply(other);
    other.type = MessageType::Hello;
    r.apply(other);
    CHECK(r.buttons == kA);
    CHECK(!r.touching && r.circle_x == 0);
}

// ---- exclusive mode (spec 013 amendment 2026-11-24: a script owns the controls) ----------------------------

namespace {

/// An Old 3DS whose Circle Pad rests at raw (10, 14) of 156, A held and the screen touched.
InputSample busy_console() {
    InputSample real;
    real.buttons = kA;
    real.circle_x = 10.0f / 156.0f;
    real.circle_y = 14.0f / 156.0f;
    real.cstick_x = -0.5f;
    real.touching = true;
    real.touch_x = 300;
    real.touch_y = 10;
    return real;
}

InputSample replaced(const RemoteInput& remote, InputSample real) {
    remote.replace_into(real);
    return real;
}

} // namespace

NNH_TEST(remote_input_exclusive_with_nothing_held_reads_an_idle_console) {
    const RemoteInput r;
    const InputSample out = replaced(r, busy_console());
    CHECK(out.buttons == 0);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.0f);
    CHECK(out.cstick_x == 0.0f && out.cstick_y == 0.0f);
    CHECK(!out.touching && out.touch_x == 0 && out.touch_y == 0);
}

NNH_TEST(remote_input_exclusive_reads_only_the_remote_controls) {
    RemoteInput r;
    r.apply(buttons(kB, 0));
    r.apply(stick(0, 0, 50)); // weaker than the real resting pad: still the remote one
    r.apply(stick(1, 250, 0));
    r.apply(touch(true, 40, 200));
    const InputSample out = replaced(r, busy_console());
    CHECK(out.buttons == kB);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.05f);
    CHECK(out.cstick_x == 0.25f && out.cstick_y == 0.0f);
    CHECK(out.touching && out.touch_x == 40 && out.touch_y == 200);
}

NNH_TEST(remote_input_exclusive_after_a_lifted_touch_reads_no_touch) {
    RemoteInput r;
    r.apply(touch(true, 40, 200));
    r.apply(touch(false, 0, 0));
    CHECK(!replaced(r, busy_console()).touching);
}

NNH_TEST(remote_input_a_resting_old_3ds_pad_is_not_at_rest) {
    InputSample real;
    real.circle_x = 10.0f / 156.0f;
    real.circle_y = 14.0f / 156.0f;
    const RestReport report = rest_report(real);
    CHECK(report.circle_moved && !report.cstick_moved && report.buttons == 0 && !report.touching);
    CHECK(!report.at_rest());
}

NNH_TEST(remote_input_sticks_inside_the_dead_zone_are_at_rest) {
    InputSample real;
    real.circle_x = 0.06f;
    real.circle_y = 0.07f; // length 0.092 < 0.10
    real.cstick_x = -0.09f;
    const RestReport report = rest_report(real);
    CHECK(!report.circle_moved && !report.cstick_moved);
    CHECK(report.at_rest());
    CHECK(rest_report(InputSample{}).at_rest());
}

NNH_TEST(remote_input_rest_report_names_every_busy_control) {
    const RestReport report = rest_report(busy_console());
    CHECK(report.circle_moved && report.cstick_moved);
    CHECK(report.buttons == kA);
    CHECK(report.touching);
    CHECK(!report.at_rest());
    InputSample only_circle_y;
    only_circle_y.circle_y = -0.2f;
    CHECK(rest_report(only_circle_y).circle_moved);
    InputSample only_circle_x;
    only_circle_x.circle_x = 0.2f;
    CHECK(rest_report(only_circle_x).circle_moved && !rest_report(only_circle_x).cstick_moved);
    InputSample only_c;
    only_c.cstick_y = 0.2f;
    CHECK(rest_report(only_c).cstick_moved && !rest_report(only_c).circle_moved && !rest_report(only_c).at_rest());
    InputSample only_button;
    only_button.buttons = kStart;
    CHECK(rest_report(only_button).buttons == kStart && !rest_report(only_button).at_rest());
    InputSample only_touch;
    only_touch.touching = true;
    CHECK(rest_report(only_touch).touching && !rest_report(only_touch).at_rest());
}

NNH_TEST(remote_input_release_all_lets_everything_go) {
    RemoteInput r;
    r.apply(buttons(kA | kB, 0));
    r.apply(stick(0, 1000, 0));
    r.apply(stick(1, 0, -1000));
    r.apply(touch(true, 1, 2));
    r.release_all();
    InputSample real;
    real.buttons = kStart;
    real.circle_y = 0.1f;
    const InputSample out = merged(r, real);
    CHECK(out.buttons == kStart);
    CHECK(out.circle_x == 0.0f && out.circle_y == 0.1f && out.cstick_y == 0.0f);
    CHECK(!out.touching);
}
