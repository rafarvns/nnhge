// The input module publishes ButtonPressed / ButtonReleased (FR-7).

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

    explicit Recorder(Log* l) : log(l) {
        pressed = nnh::events::subscribe<&Recorder::on_pressed>(this);
        released = nnh::events::subscribe<&Recorder::on_released>(this);
        ping = nnh::events::subscribe<&Recorder::on_ping>(this);
    }
    void on_pressed(const nnh::ButtonPressed& e) { log->add(static_cast<int>(bit(e.button))); }
    void on_released(const nnh::ButtonReleased& e) { log->add(-static_cast<int>(bit(e.button))); }
    void on_ping(const Ping& e) { log->add(1000 + e.value); }
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
