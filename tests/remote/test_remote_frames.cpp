// Console remote through nnh::frame() (spec 013 FR-1, FR-8): remote input enters before the frame's input is
// read, so queries and events see it on exactly the frame it was stamped for, like real input.

#include <cstring>

#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/profile.hpp"
#include "remote_fixture.hpp"

using namespace remote_test;

namespace {

constexpr std::uint32_t bit(nnh::Button b) { return static_cast<std::uint32_t>(b); }

/// A fresh engine with nnh connected and attached (Hello sent before frame 1).
void start_attached() {
    nnh::shutdown();
    fake::reset();
    fake::remote_connect();
    CHECK(nnh::init());
    send(hello());
}

struct Presses {
    int pressed = 0;
    int released = 0;
    std::uint64_t pressed_on = 0;
    std::uint64_t released_on = 0;
    nnh::Subscription on_pressed;
    nnh::Subscription on_released;

    Presses() {
        on_pressed = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed& e) {
            if (e.button != nnh::Button::A) return;
            ++pressed;
            pressed_on = nnh::frame_count();
        });
        on_released = nnh::events::subscribe<nnh::ButtonReleased>([this](const nnh::ButtonReleased& e) {
            if (e.button != nnh::Button::A) return;
            ++released;
            released_on = nnh::frame_count();
        });
    }
};

} // namespace

NNH_TEST(remote_frames_a_press_for_frame_10_is_seen_on_frame_10_only) {
    start_attached();
    send(buttons(2, 10, bit(nnh::Button::A)));
    Presses events;
    bool pressed_on[13] = {};
    for (int f = 1; f <= 12; ++f) {
        CHECK(nnh::frame());
        pressed_on[f] = nnh::input::pressed(nnh::Button::A);
    }
    CHECK(!pressed_on[9] && pressed_on[10] && !pressed_on[11]);
    CHECK(nnh::input::held(nnh::Button::A));
    CHECK(events.pressed == 1 && events.pressed_on == 10);
    nnh::shutdown();
}

NNH_TEST(remote_frames_a_hold_of_5_frames_is_held_5_frames_then_released) {
    start_attached();
    send(buttons(2, 3, bit(nnh::Button::A)));
    send(buttons(3, 8, 0, bit(nnh::Button::A)));
    Presses events;
    int held = 0;
    for (int f = 1; f <= 10; ++f) {
        CHECK(nnh::frame());
        held += nnh::input::held(nnh::Button::A) ? 1 : 0;
    }
    CHECK(held == 5);
    CHECK(events.pressed_on == 3 && events.released == 1 && events.released_on == 8);
    nnh::shutdown();
}

NNH_TEST(remote_frames_real_and_remote_buttons_add_up) {
    start_attached();
    fake::set_held_buttons(bit(nnh::Button::B));
    send(buttons(2, 0, bit(nnh::Button::A)));
    CHECK(nnh::frame());
    CHECK(nnh::input::held(nnh::Button::A) && nnh::input::held(nnh::Button::B));
    fake::set_held_buttons(0);
    CHECK(nnh::frame());
    CHECK(nnh::input::held(nnh::Button::A) && !nnh::input::held(nnh::Button::B));
    nnh::shutdown();
}

NNH_TEST(remote_frames_a_remote_stick_drives_the_dpad_mirror) {
    start_attached();
    nnh::input::set_circle_pad_mirrors_dpad(true);
    send(stick(2, 2, 0, 0, 1000)); // full up on frame 2
    CHECK(nnh::frame());
    CHECK(!nnh::input::held(nnh::Button::Up));
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(nnh::Button::Up));
    CHECK(nnh::input::circle_pad().y > 0.9f);
    nnh::input::set_circle_pad_mirrors_dpad(false);
    nnh::shutdown();
}

NNH_TEST(remote_frames_a_remote_touch_publishes_started_and_ended) {
    start_attached();
    send(touch(2, 2, true, 40, 50));
    send(touch(3, 4, false, 40, 50));
    int started = 0, ended = 0;
    nnh::TouchPoint where{};
    nnh::Subscription s1 = nnh::events::subscribe<nnh::TouchStarted>([&](const nnh::TouchStarted& e) {
        ++started;
        where = e.position;
    });
    nnh::Subscription s2 = nnh::events::subscribe<nnh::TouchEnded>([&](const nnh::TouchEnded&) { ++ended; });
    CHECK(nnh::frame());
    CHECK(started == 0);
    CHECK(nnh::frame());
    CHECK(started == 1 && where.x == 40 && where.y == 50 && nnh::input::touching());
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(ended == 1 && !nnh::input::touching());
    nnh::shutdown();
}

NNH_TEST(remote_frames_a_disconnect_publishes_button_released) {
    start_attached();
    send(buttons(2, 0, bit(nnh::Button::A)));
    Presses events;
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    fake::remote_disconnect();
    CHECK(nnh::frame());
    CHECK(!nnh::input::held(nnh::Button::A));
    CHECK(events.released == 1 && events.released_on == 3);
    nnh::shutdown();
}

NNH_TEST(remote_frames_bye_with_quit_makes_frame_return_false) {
    start_attached();
    send(bye(2, 3, true));
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(!nnh::frame());
    nnh::shutdown();
}

NNH_TEST(remote_frames_init_starts_a_new_session) {
    start_attached();
    send(bye(2, 1, true));
    CHECK(!nnh::frame());
    nnh::shutdown();
    CHECK(nnh::init()); // the same process starts the engine again: no leftover quit
    CHECK(nnh::frame());
    nnh::shutdown();
}

NNH_TEST(remote_frames_hello_is_answered_with_the_frame_it_arrives_on) {
    nnh::shutdown();
    fake::reset();
    fake::remote_connect();
    CHECK(nnh::init());
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    send(hello());
    CHECK(nnh::frame());
    const Sent s = sent();
    const Record* r = s.find(wire::RecordType::Hello);
    CHECK(r != nullptr && le32(r->payload + 4) == 3 && nnh::frame_count() == 3);
    nnh::shutdown();
}

namespace {

/// Calls of the "remote" profile scope in the previous frame.
int remote_scope_calls() {
    nnh::profile::ScopeStats top[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(top, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(top[i].name, "remote") == 0) return top[i].calls;
    }
    return 0;
}

} // namespace

NNH_TEST(remote_frames_time_the_poll_as_remote_in_the_profiler) {
    nnh::shutdown();
    fake::reset();
    fake::remote_connect();
    CHECK(nnh::init());
    CHECK(nnh::frame());
    CHECK(nnh::frame()); // the previous frame's scopes are published at the start of this one
    CHECK(remote_scope_calls() == 1);
    fake::remote_disconnect();
    CHECK(nnh::frame()); // sees the closed connection and stops polling
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(remote_scope_calls() == 0);
    nnh::shutdown();
}
