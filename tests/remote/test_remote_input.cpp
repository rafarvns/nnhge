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
