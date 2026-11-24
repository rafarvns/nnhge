// Script runner, interactive mode (remote/script_runner.hpp, spec 013 T3.5 / FR-5), against the fake console:
// lazy Hello, "now + 6", expect reports without ending, `end` detaches, errors and help are notes.

#include <string>

#include "fake_console.hpp"
#include "remote/script_runner.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
using nnh::tool::test::FakeConsole;
namespace wire = nnh::detail::remote;

namespace {

ScriptRunner interactive() { return ScriptRunner(Script{}, RunnerOptions{.interactive = true, .quit_at_end = false}); }

constexpr std::uint32_t kA = 1u << 0, kB = 1u << 1;

} // namespace

NNH_TEST(typed_mode_sends_nothing_before_the_first_command) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    c.run(30);
    CHECK(c.received.empty() && !r.hello_sent());
    r.on_typed("tap A");
    c.run(1);
    CHECK(c.received.size() == 1 && c.received[0].type == wire::MessageType::Hello);
    CHECK(!c.received.empty() && !c.received[0].exclusive); // a person typing: the console's controls still count
    c.run(20);
    CHECK(c.of_type(wire::MessageType::Buttons).size() == 2);
}

NNH_TEST(typed_input_starts_six_frames_after_the_last_frame_heard) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("wait 1"); // attaches
    c.run(20);            // Hello answered on frame 1; ticks up to 17
    c.log("something");   // frame 20 heard
    r.on_typed("tap A");
    c.run(10);
    const auto buttons = c.of_type(wire::MessageType::Buttons);
    CHECK(buttons.size() == 2 && buttons[0].frame == 26 && buttons[0].press == kA && buttons[1].frame == 27);
    CHECK(c.applied.size() == 2 && c.applied[0].frame == 26);
}

NNH_TEST(typed_input_never_goes_before_what_was_sent) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("hold A 60");
    c.run(10);            // press on 7 (frame 1 + 6), release on 67
    r.on_typed("tap B");  // now + 6 would be ~15: after the release instead
    c.run(80);
    const auto buttons = c.of_type(wire::MessageType::Buttons);
    CHECK(buttons.size() == 4);
    if (buttons.size() != 4) return;
    CHECK(buttons[1].release == kA && buttons[1].frame == 67);
    CHECK(buttons[2].press == kB && buttons[2].frame == 67 && buttons[3].frame == 68);
    CHECK(c.notes.empty() || !c.noted("late"));
}

NNH_TEST(typed_expect_reports_a_match_or_a_timeout_and_never_ends) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("tap A");
    r.on_typed("expect \"pressed 1\" 30");
    c.run(20);
    CHECK(c.noted("line 2: expect \"pressed 1\" matched at frame 7"));
    r.on_typed("expect \"nothing\" 10");
    c.logs[28].push_back("nothing"); // after the window (17-27), before the tick that ends it (29)
    c.run(20);
    CHECK(c.noted("line 3: expect \"nothing\": no matching line in frames 17-27")); // from the last tick heard

    CHECK(!r.finished() && r.verdict() == Verdict::Running);
}

NNH_TEST(typed_expect_window_includes_its_last_frame_and_ends_once) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("wait 1");
    c.run(20);                          // last tick heard: 17
    r.on_typed("expect \"last one\" 8"); // frames 17-25; a tick lands on 25
    c.logs[25].push_back("last one");
    c.run(20);
    CHECK(c.noted("line 2: expect \"last one\" matched at frame 25"));
    CHECK(!c.noted("no matching line")); // a matched expect is done
}

NNH_TEST(typed_mode_ignores_a_hello_it_did_not_ask_for) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    std::uint8_t out[wire::kMaxFixedRecord];
    c.to_pc(out, wire::encode_hello_record(3, 0x3F, "Debug", out));
    CHECK(!r.attached());
}

NNH_TEST(typed_end_detaches_and_the_next_command_attaches_again) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("press A");
    c.run(10);
    r.on_typed("end");
    c.run(2);
    CHECK(c.byes == 1 && !c.quit && !c.attached && !r.attached());
    CHECK(c.noted("detached"));
    r.on_typed("tap B");
    c.run(20);
    CHECK(c.of_type(wire::MessageType::Hello).size() == 2 && c.attached);
    CHECK(c.of_type(wire::MessageType::Buttons).size() == 3);
}

NNH_TEST(typed_mistakes_and_problems_are_notes) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("press Q");
    CHECK(c.received.empty());
    c.collect();
    CHECK(c.noted("typed:1: unknown button 'Q'"));
    r.on_typed("help");
    c.collect();
    CHECK(c.noted("expect \"TEXT\" N"));
    r.on_typed("fail-on \"oops\"");
    c.run(10);
    c.log("oops");
    CHECK(c.noted("line 3: fail-on \"oops\" matched"));
    c.log("bad", 'F');
    CHECK(!r.finished());
}

NNH_TEST(typed_late_input_is_a_note_not_the_end) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("wait 1");
    c.run(10);
    std::uint8_t out[wire::kMaxFixedRecord];
    c.to_pc(out, wire::encode_late_record(1, 5, 9, out));
    CHECK(c.noted("late") && !r.finished());
}

NNH_TEST(typed_mode_checks_controls_per_line) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    c.capabilities = 0x0F;
    r.start();
    r.on_typed("press ZL");
    c.run(10);
    CHECK(c.noted("this console has no") && c.of_type(wire::MessageType::Buttons).empty());
    r.on_typed("press A");
    c.run(10);
    CHECK(c.of_type(wire::MessageType::Buttons).size() == 1);
}

NNH_TEST(typed_screenshots_are_saved) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    r.on_typed("screenshot here");
    c.run(10);
    CHECK(c.screens.size() == 2 && c.screens[0].file_name == "frame-000007-here-top.png");
}

NNH_TEST(typed_mode_finishes_when_the_game_closes) {
    ScriptRunner r = interactive();
    FakeConsole c(r);
    r.start();
    c.run(5);
    r.on_closed();
    CHECK(r.finished() && r.exit_code() == 0);
}
