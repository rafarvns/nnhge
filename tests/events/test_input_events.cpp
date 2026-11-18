// The input module publishes ButtonPressed / ButtonReleased (spec 001 FR-7) and TouchStarted / TouchEnded
// (spec 003 FR-5).

#include "bus_fixture.hpp"
#include "nnh/input.hpp"

using bus_test::Ping;
using bus_test::restart;

namespace {

constexpr std::uint32_t bit(nnh::Button b) { return static_cast<std::uint32_t>(b); }

struct Log {
    int count = 0;
    int codes[16] = {}; // +bit for pressed, -bit for released, Ping as 1000 + value

    void add(int code) {
        if (count < 16) codes[count++] = code;
    }
};

struct Recorder {
    Log* log;
    nnh::Subscription pressed;
    nnh::Subscription released;
    nnh::Subscription ping;
    nnh::Subscription touch_started;
    nnh::Subscription touch_ended;

    explicit Recorder(Log* l) : log(l) {
        pressed = nnh::events::subscribe<&Recorder::on_pressed>(this);
        released = nnh::events::subscribe<&Recorder::on_released>(this);
        ping = nnh::events::subscribe<&Recorder::on_ping>(this);
        touch_started = nnh::events::subscribe<&Recorder::on_touch_started>(this);
        touch_ended = nnh::events::subscribe<&Recorder::on_touch_ended>(this);
    }
    void on_pressed(const nnh::ButtonPressed& e) { log->add(static_cast<int>(bit(e.button))); }
    void on_released(const nnh::ButtonReleased& e) { log->add(-static_cast<int>(bit(e.button))); }
    void on_ping(const Ping& e) { log->add(1000 + e.value); }
    void on_touch_started(const nnh::TouchStarted& e) { log->add(touch_code(2, e.position)); }
    void on_touch_ended(const nnh::TouchEnded& e) { log->add(touch_code(3, e.position)); }
    /// TouchStarted as 2xxxyyy, TouchEnded as 3xxxyyy.
    static int touch_code(int kind, nnh::TouchPoint p) { return kind * 1000000 + p.x * 1000 + p.y; }
};

} // namespace

NNH_TEST(a_button_going_down_publishes_one_pressed_event) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::A));
    CHECK(nnh::frame());
    CHECK(log.count == 1 && log.codes[0] == static_cast<int>(bit(nnh::Button::A)));
    CHECK(nnh::frame()); // still held: no new event
    CHECK(log.count == 1);
    nnh::shutdown();
}

NNH_TEST(a_button_going_up_publishes_one_released_event) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::B));
    CHECK(nnh::frame());
    nnh::platform_fake::set_held_buttons(0);
    CHECK(nnh::frame());
    CHECK(log.count == 2 && log.codes[1] == -static_cast<int>(bit(nnh::Button::B)));
    CHECK(nnh::frame());
    CHECK(log.count == 2);
    nnh::shutdown();
}

NNH_TEST(presses_come_before_releases_each_in_button_order) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::L) | bit(nnh::Button::Start));
    CHECK(nnh::frame());
    log.count = 0;
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::Right) | bit(nnh::Button::A));
    CHECK(nnh::frame());
    CHECK(log.count == 4);
    CHECK(log.codes[0] == static_cast<int>(bit(nnh::Button::A)));
    CHECK(log.codes[1] == static_cast<int>(bit(nnh::Button::Right)));
    CHECK(log.codes[2] == -static_cast<int>(bit(nnh::Button::L)));
    CHECK(log.codes[3] == -static_cast<int>(bit(nnh::Button::Start)));
    nnh::shutdown();
}

NNH_TEST(input_events_agree_with_the_input_queries) {
    restart();
    bool agreed = false;
    nnh::Subscription sub = nnh::events::subscribe<nnh::ButtonPressed>([&agreed](const nnh::ButtonPressed& e) {
        agreed = nnh::input::pressed(e.button) && nnh::input::held(e.button);
    });
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::Y));
    CHECK(nnh::frame());
    CHECK(agreed);
    nnh::shutdown();
}

NNH_TEST(game_events_from_the_previous_frame_come_before_this_frames_input) {
    restart();
    Log log;
    Recorder rec(&log);
    CHECK(nnh::frame());
    nnh::events::publish(Ping{1});
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::X));
    CHECK(nnh::frame());
    CHECK(log.count == 2 && log.codes[0] == 1001 && log.codes[1] == static_cast<int>(bit(nnh::Button::X)));
    nnh::shutdown();
}

NNH_TEST(a_touch_publishes_started_once_with_its_position) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_touch(120, 45);
    CHECK(nnh::frame());
    nnh::platform_fake::set_touch(130, 50); // moving: no event
    CHECK(nnh::frame());
    CHECK(log.count == 1 && log.codes[0] == 2120045);
    nnh::shutdown();
}

NNH_TEST(a_touch_ending_publishes_ended_once_with_the_last_point) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_touch(10, 20);
    CHECK(nnh::frame());
    nnh::platform_fake::set_touch(30, 40);
    CHECK(nnh::frame());
    nnh::platform_fake::release_touch();
    CHECK(nnh::frame());
    CHECK(nnh::frame());
    CHECK(log.count == 2 && log.codes[1] == 3030040);
    nnh::shutdown();
}

NNH_TEST(touch_events_come_after_button_events) {
    restart();
    Log log;
    Recorder rec(&log);
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::B));
    CHECK(nnh::frame());
    log.count = 0;
    nnh::platform_fake::set_held_buttons(bit(nnh::Button::A));
    nnh::platform_fake::set_touch(1, 2);
    CHECK(nnh::frame());
    CHECK(log.count == 3);
    CHECK(log.codes[0] == static_cast<int>(bit(nnh::Button::A)));
    CHECK(log.codes[1] == -static_cast<int>(bit(nnh::Button::B)));
    CHECK(log.codes[2] == 2001002);
    nnh::shutdown();
}

NNH_TEST(touch_events_agree_with_the_touch_queries) {
    restart();
    bool agreed = false;
    nnh::Subscription sub = nnh::events::subscribe<nnh::TouchStarted>([&agreed](const nnh::TouchStarted& e) {
        agreed = nnh::input::touch_started() && nnh::input::touching() &&
                 nnh::input::touch_position().x == e.position.x && nnh::input::touch_position().y == e.position.y;
    });
    nnh::platform_fake::set_touch(200, 100);
    CHECK(nnh::frame());
    CHECK(agreed);
    nnh::shutdown();
}
