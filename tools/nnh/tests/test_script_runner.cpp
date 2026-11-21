// Script runner, script mode (remote/script_runner.hpp, spec 013 T3.5), against the fake console of
// fake_console.hpp: Hello, frame stamps, the send window, expect barriers and windows, fail-on, level F,
// Late/Rejected/Dropped, screenshots, end, capabilities, the game closing, verdicts and exit codes.

#include <string>

#include "fake_console.hpp"
#include "remote/script_runner.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
using nnh::tool::test::FakeConsole;
namespace wire = nnh::detail::remote;

namespace {

Script script_of(const std::string& text) {
    ParsedScript p = parse_script(text, "t.nnhs");
    return p.script;
}

ScriptRunner runner_for(const std::string& text, bool quit = false) {
    return ScriptRunner(script_of(text), RunnerOptions{.interactive = false, .quit_at_end = quit});
}

bool buttons_at(const FakeConsole& c, std::uint32_t frame, std::uint32_t press, std::uint32_t release) {
    for (const auto& a : c.applied) {
        if (a.message.type == wire::MessageType::Buttons && a.frame == frame && a.message.frame == frame &&
            a.message.press == press && a.message.release == release) {
            return true;
        }
    }
    return false;
}

constexpr std::uint32_t kA = 1u << 0, kB = 1u << 1;

} // namespace

NNH_TEST(runner_sends_hello_first_and_nothing_before_its_answer) {
    ScriptRunner r = runner_for("tap A\n");
    CHECK(r.take_messages().empty()); // not before the game connects
    r.start();
    const auto sent = r.take_messages();
    CHECK(sent.size() == 1);
    wire::Message m;
    CHECK(sent.size() == 1 && wire::decode_message(sent[0].data(), m) == wire::Reject::None);
    CHECK(m.type == wire::MessageType::Hello && m.version == wire::kVersion && m.frame == 0);
    CHECK(r.hello_sent() && !r.attached() && r.verdict() == Verdict::Running);
    r.on_line("[f1 0.0s] I app started");
    CHECK(r.take_messages().empty());
}

NNH_TEST(runner_stamps_inputs_with_script_frames_from_frame_1) {
    ScriptRunner r = runner_for("wait 30\ntap A\nhold B 5\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished();
    CHECK(buttons_at(c, 31, kA, 0) && buttons_at(c, 32, 0, kA));
    CHECK(buttons_at(c, 33, kB, 0) && buttons_at(c, 38, 0, kB));
    CHECK(r.verdict() == Verdict::Pass && r.exit_code() == 0);
}

NNH_TEST(runner_passes_once_a_tick_reaches_the_final_clock) {
    ScriptRunner r = runner_for("wait 30\nend\n");
    FakeConsole c(r);
    r.start();
    c.run(30);
    CHECK(r.verdict() == Verdict::Running);
    c.run(1); // frame 31 starts: no tick on it (ticks: 1, 5, ... 29, 33)
    CHECK(r.verdict() == Verdict::Running);
    c.run(2);
    CHECK(r.verdict() == Verdict::Pass && r.finished());
    CHECK(c.noted("PASS (2 lines, frames 1-31)"));
}

NNH_TEST(runner_says_bye_at_the_end_and_asks_to_quit_only_when_told) {
    for (bool quit : {false, true}) {
        ScriptRunner r = runner_for("wait 2\n", quit);
        FakeConsole c(r);
        r.start();
        c.run_until_finished();
        c.run(2);
        CHECK(c.byes == 1 && c.quit == quit);
    }
}

NNH_TEST(runner_keeps_at_most_48_messages_unapplied) {
    ScriptRunner r = runner_for("wait 10\ntouch 0 0\ndrag 300 200 200\nuntouch\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished();
    CHECK(r.verdict() == Verdict::Pass);
    CHECK(c.most_waiting == kMaxUnapplied);
    CHECK(c.waiting_at[98] == kMaxUnapplied); // topped up after the tick of frame 97: frames 98-145 wait);
    CHECK(c.of_type(wire::MessageType::Touch).size() == 202);
}

NNH_TEST(runner_sends_nothing_after_an_expect_before_it_matches) {
    ScriptRunner r = runner_for("wait 30\ntap A\nexpect \"never\" 100\nwait 20\ntap B\n");
    FakeConsole c(r);
    r.start();
    c.run(60);
    CHECK(c.of_type(wire::MessageType::Buttons).size() == 2); // tap A only
}

NNH_TEST(runner_continues_from_the_frame_after_the_matching_line) {
    ScriptRunner r = runner_for("wait 30\ntap A\nexpect \"pressed 1\" 10\nwait 20\ntap B\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished();
    CHECK(r.verdict() == Verdict::Pass);
    // "pressed 1" (A) is logged on frame 31: the clock is 32, then wait 20 → B on frame 52.
    CHECK(buttons_at(c, 52, kB, 0) && buttons_at(c, 53, 0, kB));
    CHECK(c.noted("line 3: expect \"pressed 1\" matched at frame 31"));
}

NNH_TEST(runner_expect_window_starts_at_the_segment_and_ends_n_frames_after_the_clock) {
    ScriptRunner early = runner_for("wait 30\nexpect \"ready\" 5\n");
    FakeConsole a(early);
    a.logs[1].push_back("ready"); // frame 1: inside (the first segment starts there)
    early.start();
    a.run_until_finished(200);
    CHECK(early.verdict() == Verdict::Pass);

    ScriptRunner late = runner_for("wait 30\nexpect \"ready\" 5\n");
    FakeConsole b(late);
    b.logs[36].push_back("ready"); // clock 31 + 5 = 36: the last frame of the window
    late.start();
    b.run_until_finished(200);
    CHECK(late.verdict() == Verdict::Pass);
}

NNH_TEST(runner_expect_ignores_lines_from_before_its_segment) {
    ScriptRunner r = runner_for("wait 10\nexpect \"a\" 5\nwait 10\nexpect \"a\" 5\n");
    FakeConsole c(r);
    c.logs[3] = {"a", "a"}; // the second "a" is on the matching frame, before the next segment
    r.start();
    c.run_until_finished(200);
    CHECK(r.verdict() == Verdict::Fail && r.exit_code() == 1);
    CHECK(c.noted("FAIL at t.nnhs:4: expect \"a\": no matching line in frames 4-19"));
}

NNH_TEST(runner_expect_times_out_at_the_first_tick_past_its_window) {
    ScriptRunner r = runner_for("wait 10\nexpect \"x\" 10\n");
    FakeConsole c(r);
    r.start();
    c.run(21);  // window 1-21; tick on 21 doesn't end it
    CHECK(r.verdict() == Verdict::Running);
    c.run(4);   // tick on 25
    CHECK(r.verdict() == Verdict::Fail);
    CHECK(c.noted("FAIL at t.nnhs:2: expect \"x\": no matching line in frames 1-21"));
    c.logs[30].push_back("x");
    c.run(10);
    CHECK(r.verdict() == Verdict::Fail); // nothing changes after a verdict
}

NNH_TEST(runner_fails_on_a_fail_on_match_from_its_frame_on) {
    ScriptRunner r = runner_for("wait 20\nfail-on \"bad*thing\"\nwait 20\n");
    FakeConsole c(r);
    c.logs[10].push_back("bad thing"); // before the fail-on's frame (21)
    c.logs[30].push_back("a bad old thing");
    r.start();
    c.run(29);
    CHECK(r.verdict() == Verdict::Running);
    c.run(1);
    CHECK(r.verdict() == Verdict::Fail);
    CHECK(c.noted("FAIL at t.nnhs:2: fail-on \"bad*thing\" matched: [f30"));
}

NNH_TEST(runner_gives_lines_without_a_prefix_the_last_tick_frame) {
    ScriptRunner r = runner_for("wait 20\nfail-on \"boom\"\nwait 20\n");
    FakeConsole c(r);
    r.start();
    c.run(18);                // last tick: 17
    c.to_pc_line("boom");     // counts as frame 17: before the fail-on
    CHECK(r.verdict() == Verdict::Running);
    c.run(4);                 // tick 21
    c.to_pc_line("boom");
    CHECK(r.verdict() == Verdict::Fail);
}

NNH_TEST(runner_fails_on_a_fatal_line) {
    ScriptRunner r = runner_for("wait 50\n");
    FakeConsole c(r);
    r.start();
    c.run(5);
    c.log("assert failed: x", 'F');
    CHECK(r.verdict() == Verdict::Fail);
    CHECK(c.noted("FAIL at t.nnhs:1: the game logged a fatal error: [f5"));
}

NNH_TEST(runner_fails_on_late_input_with_its_line) {
    ScriptRunner r = runner_for("# comment\ntap A\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished(50); // Hello answered on frame 1; the press for frame 1 arrives on frame 2
    CHECK(r.verdict() == Verdict::Fail);
    CHECK(c.noted("FAIL at t.nnhs:2: the input for frame 1 reached the console on frame 2 (late)"));
}

NNH_TEST(runner_fails_on_rejected_and_dropped_messages) {
    for (int dropped = 0; dropped < 2; ++dropped) {
        ScriptRunner r = runner_for("wait 30\npress A\nwait 30\n");
        FakeConsole c(r);
        r.start();
        c.run(3);
        const wire::Message press = c.of_type(wire::MessageType::Buttons).at(0);
        std::uint8_t out[wire::kMaxFixedRecord];
        const std::size_t n = dropped ? wire::encode_dropped_record(press.seq, out)
                                      : wire::encode_rejected_record(press.seq, wire::Reject::Range, out);
        c.to_pc(out, n);
        CHECK(r.verdict() == Verdict::Fail);
        CHECK(c.noted(dropped ? "FAIL at t.nnhs:2: the console's queue was full (message dropped)"
                              : "FAIL at t.nnhs:2: the console rejected the message (value out of range)"));
    }
}

NNH_TEST(runner_checks_capabilities_before_sending_any_input) {
    for (const char* text : {"wait 30\ntap A\npress zl\n", "wait 30\nstick cstick 1 0\n"}) {
        ScriptRunner r = runner_for(text);
        FakeConsole c(r);
        c.capabilities = 0x0F; // an Old 3DS: no C-Stick, no ZL/ZR
        r.start();
        c.run(40);
        CHECK(r.verdict() == Verdict::Fail);
        CHECK(c.of_type(wire::MessageType::Buttons).empty() && c.of_type(wire::MessageType::Stick).empty());
        CHECK(c.noted("this console has no"));
    }
    ScriptRunner fine = runner_for("wait 30\nrelease all\n");
    FakeConsole c(fine);
    c.capabilities = 0x0F; // release all centres the C-Stick too, but asks for nothing
    fine.start();
    c.run_until_finished(100);
    CHECK(fine.verdict() == Verdict::Pass);
}

NNH_TEST(runner_waits_for_both_screens_and_names_the_files) {
    ScriptRunner r = runner_for("wait 60\nscreenshot stick-right\n");
    FakeConsole c(r);
    r.start();
    c.run_until_finished(200);
    CHECK(r.verdict() == Verdict::Pass);
    CHECK(c.screens.size() == 2);
    if (c.screens.size() != 2) return;
    CHECK(c.screens[0].file_name == "frame-000061-stick-right-top.png" && c.screens[0].line == 2);
    CHECK(c.screens[1].file_name == "frame-000061-stick-right-bottom.png" && c.screens[1].name == "stick-right");
    CHECK(c.screens[0].image.width == 400 && c.screens[1].image.width == 320);
}

NNH_TEST(runner_end_waits_for_a_screenshot_still_on_its_way) {
    ScriptRunner r = runner_for("wait 4\nscreenshot s\n");
    FakeConsole c(r);
    r.start();
    c.run(4);
    std::uint8_t out[wire::kMaxFixedRecord];
    c.to_pc(out, wire::encode_tick_record(5, out)); // the final clock, before the screens of frame 5
    CHECK(r.verdict() == Verdict::Running);
    c.run(1);
    CHECK(r.verdict() == Verdict::Pass);
}

NNH_TEST(runner_fails_on_a_failed_or_bad_screenshot) {
    ScriptRunner failed = runner_for("wait 60\nscreenshot s\nwait 10\n");
    FakeConsole a(failed);
    failed.start();
    a.run(3);
    const wire::Message shot = a.of_type(wire::MessageType::Screenshot).at(0);
    std::uint8_t out[wire::kMaxFixedRecord];
    a.to_pc(out, wire::encode_screen_failed_record(shot.id, 0, 61, out));
    CHECK(failed.verdict() == Verdict::Fail && a.noted("FAIL at t.nnhs:2: the console couldn't take screenshot s"));

    ScriptRunner bad = runner_for("wait 60\nscreenshot s\nwait 10\n");
    FakeConsole b(bad);
    bad.start();
    b.run(3);
    const wire::Message shot2 = b.of_type(wire::MessageType::Screenshot).at(0);
    const wire::ScreenHeader h{.id = shot2.id, .screen = 0, .layout = 1, .frame = 61, .width = 400, .height = 240};
    std::vector<std::uint8_t> bytes(out, out + wire::encode_screen_record(h, 10, out));
    bytes.resize(bytes.size() + 10);
    b.to_pc(bytes.data(), bytes.size());
    CHECK(bad.verdict() == Verdict::Fail && b.noted("FAIL at t.nnhs:2: screenshot s: the top screen should be"));
}

NNH_TEST(runner_fails_when_the_game_closes_before_the_end) {
    ScriptRunner r = runner_for("wait 30\nexpect \"x\" 50\n");
    FakeConsole c(r);
    r.start();
    c.run(10);
    r.on_closed();
    c.collect();
    CHECK(r.verdict() == Verdict::Fail && r.finished() && r.exit_code() == 1);
    CHECK(c.noted("FAIL at t.nnhs:2: the game closed before the script ended"));
    CHECK(r.take_messages().empty()); // no Bye on a closed connection
}

NNH_TEST(runner_cannot_run_without_an_answer) {
    ScriptRunner quiet = runner_for("wait 1\n");
    quiet.start();
    quiet.no_answer();
    CHECK(quiet.verdict() == Verdict::Error && quiet.exit_code() == 2);
    CHECK(quiet.take_notes().at(0) == "the game didn't answer: a Release build, or one older than nnh");

    ScriptRunner gone = runner_for("wait 1\n");
    gone.start();
    gone.on_closed();
    CHECK(gone.verdict() == Verdict::Error && gone.exit_code() == 2);

    ScriptRunner other = runner_for("wait 1\n");
    other.start();
    std::uint8_t out[wire::kMaxFixedRecord];
    wire::encode_hello_record(1, 0, "Debug", out);
    out[wire::kRecordHeaderSize] = 9; // version 9
    FakeConsole c(other);
    c.to_pc(out, wire::kRecordHeaderSize + wire::kHelloPayload);
    CHECK(other.verdict() == Verdict::Error && c.noted("version 9"));
}
