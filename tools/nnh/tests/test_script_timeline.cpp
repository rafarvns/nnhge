// Script timeline (remote/script_timeline.hpp, spec 013 T3.4): steps → frame-stamped messages, the clock,
// hold/tap/drag expansion, release all, screenshots, and segments cut at each expect.

#include <string>
#include <vector>

#include "remote/script_timeline.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;

namespace {

std::vector<Segment> timeline(const std::string& text) {
    const ParsedScript p = parse_script(text, "t.nnhs");
    return p.errors.empty() ? build_timeline(p.script) : std::vector<Segment>{};
}

bool is_buttons(const TimedEvent& e, std::uint32_t offset, std::uint32_t press, std::uint32_t release) {
    return e.kind == TimedEvent::Kind::Send && e.message.type == wire::MessageType::Buttons &&
           e.offset == offset && e.message.press == press && e.message.release == release;
}

bool is_touch(const TimedEvent& e, std::uint32_t offset, bool down, int x, int y) {
    return e.message.type == wire::MessageType::Touch && e.offset == offset && e.message.down == down &&
           e.message.x == x && e.message.y == y;
}

constexpr std::uint32_t kA = 1u << 0, kB = 1u << 1;

} // namespace

NNH_TEST(timeline_press_and_release_happen_at_the_clock) {
    const auto s = timeline("press A\nwait 10\nrelease A\nwait 5\n");
    CHECK(s.size() == 1);
    if (s.size() != 1) return;
    CHECK(s[0].events.size() == 2 && s[0].length == 15);
    CHECK(s[0].events.size() == 2 && is_buttons(s[0].events[0], 0, kA, 0) && is_buttons(s[0].events[1], 10, 0, kA));
    CHECK(s[0].events.size() == 2 && s[0].events[0].line == 1 && s[0].events[1].line == 3);
    CHECK(s[0].until.command == Command::End && s[0].until.line == 4);
}

NNH_TEST(timeline_hold_releases_n_frames_later) {
    const auto s = timeline("wait 3\nhold A+B 20\npress A");
    CHECK(s.size() == 1 && s[0].events.size() == 3 && s[0].length == 23);
    if (s.size() != 1 || s[0].events.size() != 3) return;
    CHECK(is_buttons(s[0].events[0], 3, kA | kB, 0) && is_buttons(s[0].events[1], 23, 0, kA | kB));
    CHECK(is_buttons(s[0].events[2], 23, kA, 0));
}

NNH_TEST(timeline_tap_releases_on_the_next_frame_and_takes_two) {
    const auto s = timeline("tap A\ntap A\n");
    CHECK(s.size() == 1 && s[0].events.size() == 4 && s[0].length == 4);
    if (s.size() != 1 || s[0].events.size() != 4) return;
    CHECK(is_buttons(s[0].events[0], 0, kA, 0) && is_buttons(s[0].events[1], 1, 0, kA));
    CHECK(is_buttons(s[0].events[2], 2, kA, 0) && is_buttons(s[0].events[3], 3, 0, kA));
}

NNH_TEST(timeline_sets_sticks) {
    const auto s = timeline("stick cstick -1 0.5\n");
    CHECK(s.size() == 1 && s[0].events.size() == 1 && s[0].length == 0);
    if (s.size() != 1 || s[0].events.size() != 1) return;
    const wire::Message& m = s[0].events[0].message;
    CHECK(m.type == wire::MessageType::Stick && m.stick == 1 && m.x == -1000 && m.y == 500);
}

NNH_TEST(timeline_drag_moves_one_step_per_frame_after_the_touch) {
    const auto s = timeline("touch 0 10\ndrag 30 40 3\n");
    CHECK(s.size() == 1 && s[0].events.size() == 4);
    if (s.size() != 1 || s[0].events.size() != 4) return;
    CHECK(is_touch(s[0].events[0], 0, true, 0, 10));
    CHECK(is_touch(s[0].events[1], 1, true, 10, 20));
    CHECK(is_touch(s[0].events[2], 2, true, 20, 30));
    CHECK(is_touch(s[0].events[3], 3, true, 30, 40));
}

NNH_TEST(timeline_drag_holds_its_target_one_frame_before_what_follows) {
    const auto s = timeline("touch 0 10\ndrag 30 40 3\nuntouch\n");
    CHECK(s.size() == 1 && s[0].events.size() == 5 && s[0].length == 4);
    if (s.size() != 1 || s[0].events.size() != 5) return;
    CHECK(is_touch(s[0].events[3], 3, true, 30, 40));
    CHECK(is_touch(s[0].events[4], 4, false, 30, 40) && s[0].events[4].line == 3);
}

NNH_TEST(timeline_drag_ends_exactly_on_its_target) {
    const auto s = timeline("touch 0 0\ndrag 10 7 3\ndrag 10 0 1\n");
    CHECK(s.size() == 1 && s[0].events.size() == 5);
    if (s.size() != 1 || s[0].events.size() != 5) return;
    CHECK(is_touch(s[0].events[3], 3, true, 10, 7));
    CHECK(is_touch(s[0].events[4], 5, true, 10, 0)); // the next drag starts where the last one ended
}

NNH_TEST(timeline_release_all_lets_every_control_go) {
    const auto s = timeline("wait 2\nrelease all\n");
    CHECK(s.size() == 1 && s[0].events.size() == 4);
    if (s.size() != 1 || s[0].events.size() != 4) return;
    CHECK(is_buttons(s[0].events[0], 2, 0, wire::kButtonMask));
    const wire::Message& circle = s[0].events[1].message;
    const wire::Message& cstick = s[0].events[2].message;
    CHECK(circle.type == wire::MessageType::Stick && circle.stick == 0 && circle.x == 0 && circle.y == 0);
    CHECK(cstick.type == wire::MessageType::Stick && cstick.stick == 1 && s[0].events[2].offset == 2);
    CHECK(is_touch(s[0].events[3], 2, false, 0, 0));
}

NNH_TEST(timeline_screenshot_asks_for_both_screens_with_its_name) {
    const auto s = timeline("wait 7\nscreenshot dragged\n");
    CHECK(s.size() == 1 && s[0].events.size() == 1);
    if (s.size() != 1 || s[0].events.size() != 1) return;
    const TimedEvent& e = s[0].events[0];
    CHECK(e.message.type == wire::MessageType::Screenshot && e.offset == 7 && e.text == "dragged");
    CHECK(e.message.screens == (wire::kScreenTop | wire::kScreenBottom) && e.line == 2);
}

NNH_TEST(timeline_screenshot_takes_a_frame_so_what_follows_is_not_in_it) {
    const auto s = timeline("wait 7\nscreenshot a\nstick circle 0 0\nscreenshot b\nscreenshot c\n");
    CHECK(s.size() == 1 && s[0].events.size() == 4 && s[0].length == 10);
    if (s.size() != 1 || s[0].events.size() != 4) return;
    CHECK(s[0].events[0].offset == 7 && s[0].events[1].offset == 8); // the stick moves after frame 7's capture
    CHECK(s[0].events[2].offset == 8 && s[0].events[3].offset == 9);
}

NNH_TEST(timeline_fail_on_is_an_event_at_the_clock) {
    const auto s = timeline("wait 4\nfail-on \"] E \"\n");
    CHECK(s.size() == 1 && s[0].events.size() == 1);
    if (s.size() != 1 || s[0].events.size() != 1) return;
    CHECK(s[0].events[0].kind == TimedEvent::Kind::FailOn && s[0].events[0].offset == 4);
    CHECK(s[0].events[0].text == "] E " && s[0].events[0].line == 2);
}

NNH_TEST(timeline_is_cut_at_each_expect_and_restarts_its_clock) {
    const auto s = timeline("wait 30\ntap A\nexpect \"pressed A\" 10\nwait 10\ntap B\nexpect \"x\" 5\nend\n");
    CHECK(s.size() == 3);
    if (s.size() != 3) return;
    CHECK(s[0].length == 32 && s[0].until.command == Command::Expect && s[0].until.line == 3);
    CHECK(s[0].until.text == "pressed A" && s[0].until.frames == 10);
    CHECK(s[1].length == 12 && s[1].events.size() == 2 && is_buttons(s[1].events[0], 10, kB, 0));
    CHECK(s[1].until.command == Command::Expect && s[1].until.line == 6);
    CHECK(s[2].length == 0 && s[2].events.empty() && s[2].until.command == Command::End && s[2].until.line == 7);
}

NNH_TEST(timeline_hands_out_what_was_typed_since_the_last_take) {
    Timeline t;
    Step touch;
    touch.command = Command::Touch;
    touch.x = 100;
    touch.y = 50;
    t.add(touch);
    const Segment first = t.take_open();
    CHECK(first.events.size() == 1 && first.length == 0);
    Step drag;
    drag.command = Command::Drag;
    drag.frames = 2;
    drag.x = 110;
    drag.y = 50;
    t.add(drag);
    const Segment second = t.take_open();
    CHECK(second.events.size() == 2 && second.length == 3);
    CHECK(second.events.size() == 2 && is_touch(second.events[0], 1, true, 105, 50));
    CHECK(t.take_open().events.empty());
}
