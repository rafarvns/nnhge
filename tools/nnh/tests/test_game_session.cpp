// The session loop (remote/game_session.hpp, spec 013 T4.3): a real loopback connection to a fake game
// (fake_game.hpp). Terminal and log file are temporary files read back after the session.

#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include "fake_game.hpp"
#include "remote/game_session.hpp"
#include "remote/script_runner.hpp"
#include "remote/stdin_lines.hpp"
#include "test.hpp"

using namespace nnh::tool;
using nnh::tool::test::FakeGame;
namespace rr = nnh::tool::remote;

namespace {

const Ipv4 kLoopback{{127, 0, 0, 1}};

std::string read_back(std::FILE* f) {
    std::string text;
    std::rewind(f);
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    return text;
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

/// A session with `config` against `game`, which connects from a thread. Fills `out`/`log` with what was
/// written to the terminal and the log file.
struct Run {
    std::string out;
    std::string log;
    int code = -1;
};

Run run_session(FakeGame& game, SessionConfig config) {
    net::Socket listener = net::tcp_listen(kLoopback, 0);
    std::thread thread([&game, port = listener.local_port()] { game.run(port); });
    net::Socket connection = net::tcp_accept(listener, 2000);
    std::FILE* out = std::tmpfile();
    std::FILE* log = std::tmpfile();
    config.out = out;
    config.log = log;
    Run r;
    r.code = run_game_session(connection, config);
    connection.close();
    thread.join();
    r.out = read_back(out);
    r.log = read_back(log);
    std::fclose(out);
    std::fclose(log);
    return r;
}

std::string shots_dir(const char* name) {
    const std::string dir = std::string(NNH_TOOL_TEST_TMP) + "/" + name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return dir;
}

rr::Script script_of(const char* text) {
    rr::ParsedScript parsed = rr::parse_script(text, "t.nnhs");
    return parsed.script;
}

} // namespace

NNH_TEST(session_shows_and_saves_every_line_of_a_plain_log) {
    FakeGame game;
    game.frames = 0;
    game.first_lines = {"[f1        0.000s] I app    started\r\nsecond\n\nlast without newline"};
    const Run r = run_session(game, SessionConfig{});
    CHECK(r.code == 0);
    const std::string lines = "[f1        0.000s] I app    started\nsecond\n\nlast without newline\n";
    CHECK(r.out == lines);
    CHECK(r.log == lines);
    CHECK(game.received.empty()); // no runner: nothing is sent
}

NNH_TEST(session_runs_a_script_to_pass_and_saves_its_screenshots) {
    FakeGame game;
    game.tick_every = 1; // the final tick comes before the screens: the last screen completes the script
    rr::ScriptRunner runner(script_of("wait 5\ntap A\nexpect \"pressed\" 20\nscreenshot shot\nend\n"),
                            rr::RunnerOptions{.interactive = false, .quit_at_end = true});
    SessionConfig config;
    config.runner = &runner;
    config.shots_dir = shots_dir("session_pass");
    config.linger_ms = 2000;
    const Run r = run_session(game, config);
    CHECK(r.code == 0);
    CHECK(has(r.out, "game   pressed 1\n") && has(r.out, "nnh: line 3: expect \"pressed\" matched at frame"));
    CHECK(has(r.out, "nnh: PASS (5 lines, frames 1-"));
    CHECK(has(r.log, "nnh: PASS (5 lines, frames 1-") && has(r.log, "game   pressed 1\n"));
    CHECK(has(r.out, "game   bye\n")); // the log after the verdict is shown until the game closes
    CHECK(has(r.out, "nnh: line 4 screenshot shot -> " + config.shots_dir + "/frame-"));
    CHECK(r.out.rfind("screenshot shot -> ") < r.out.find("nnh: PASS")); // every file first, then the verdict
    int pngs = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(config.shots_dir, ec)) {
        const std::string name = entry.path().filename().string();
        pngs += name.find("-shot-top.png") != std::string::npos || name.find("-shot-bottom.png") != std::string::npos;
        CHECK(entry.file_size() > 320u * 240u * 3u); // at least 320×240 RGB, stored uncompressed
    }
    CHECK(pngs == 2);
    CHECK(!game.received.empty() && game.received.back().type == nnh::detail::remote::MessageType::Bye);
}

NNH_TEST(session_gives_up_when_the_game_does_not_answer) {
    FakeGame game;
    game.answer_hello = false;
    rr::ScriptRunner runner(script_of("wait 5\nend\n"), rr::RunnerOptions{});
    SessionConfig config;
    config.runner = &runner;
    config.hello_timeout_ms = 200;
    config.linger_ms = 0;
    const Run r = run_session(game, config);
    CHECK(r.code == 2);
    CHECK(has(r.out, "nnh: the game didn't answer"));
    CHECK(game.received.size() == 1); // the Hello, and no Bye to a game that never attached
}

// A game loading for seconds before its first frame (examples/data on an Old 3DS: ~2-4 s) answers late; every log
// line it sends meanwhile shows it's alive, so the wait starts again.
NNH_TEST(session_waits_for_a_game_that_logs_while_it_loads) {
    FakeGame game;
    game.init_lines = 4;
    game.init_gap_ms = 80; // 320 ms of loading, each gap under the 150 ms timeout
    rr::ScriptRunner runner(script_of("wait 5\nend\n"), rr::RunnerOptions{});
    SessionConfig config;
    config.runner = &runner;
    config.hello_timeout_ms = 150;
    config.linger_ms = 0;
    const Run r = run_session(game, config);
    CHECK(r.code == 0);
    CHECK(!has(r.out, "didn't answer"));
    CHECK(has(r.out, "game   loading 3\n") && has(r.out, "nnh: PASS"));
}

NNH_TEST(the_default_wait_for_an_answer_allows_a_slow_start) {
    CHECK(SessionConfig{}.hello_timeout_ms >= 10000);
}

NNH_TEST(typed_commands_to_a_game_that_does_not_answer_leave_its_log_running) {
    FakeGame game;
    game.answer_hello = false;
    game.frames = 200;
    game.late_line_frame = 150; // ~0.3 s in: after the 100 ms timeout
    std::FILE* in = std::tmpfile();
    std::fputs("tap A\n", in);
    std::rewind(in);
    StdinLines typed;
    typed.start(in);
    rr::ScriptRunner runner(rr::Script{}, rr::RunnerOptions{.interactive = true, .quit_at_end = false});
    SessionConfig config;
    config.runner = &runner;
    config.typed = &typed;
    config.hello_timeout_ms = 100;
    const Run r = run_session(game, config);
    std::fclose(in);
    CHECK(r.code == 0);
    CHECK(has(r.out, "nnh: the game didn't answer"));
    CHECK(has(r.out, "game   late line\n")); // interactive mode follows the log until the game closes
}

NNH_TEST(an_untouched_interactive_session_never_waits_for_an_answer) {
    FakeGame game;
    game.frames = 150; // ~0.3 s, well past the timeout below
    std::FILE* in = std::tmpfile(); // nothing typed
    StdinLines typed;
    typed.start(in);
    rr::ScriptRunner runner(rr::Script{}, rr::RunnerOptions{.interactive = true, .quit_at_end = false});
    SessionConfig config;
    config.runner = &runner;
    config.typed = &typed;
    config.hello_timeout_ms = 50;
    const Run r = run_session(game, config);
    std::fclose(in);
    CHECK(r.code == 0 && game.received.empty());
    CHECK(!has(r.out, "didn't answer")); // no Hello was sent, so nothing to answer
}

NNH_TEST(session_fails_when_the_game_closes_before_the_end) {
    FakeGame game;
    game.close_after_hello = 3;
    rr::ScriptRunner runner(script_of("wait 100\nend\n"), rr::RunnerOptions{});
    SessionConfig config;
    config.runner = &runner;
    const Run r = run_session(game, config);
    CHECK(r.code == 1);
    CHECK(has(r.out, "nnh: FAIL at t.nnhs:2: the game closed before the script ended"));
}

NNH_TEST(session_sends_typed_commands_and_keeps_the_log_after_end) {
    FakeGame game;
    game.frames = 150;
    std::FILE* in = std::tmpfile();
    std::fputs("help\ntap A\nexpect \"pressed\" 30\n", in);
    std::rewind(in);
    StdinLines typed;
    typed.start(in);
    rr::ScriptRunner runner(rr::Script{}, rr::RunnerOptions{.interactive = true, .quit_at_end = false});
    SessionConfig config;
    config.runner = &runner;
    config.typed = &typed;
    const Run r = run_session(game, config);
    std::fclose(in);
    CHECK(r.code == 0);
    CHECK(has(r.out, "nnh: commands (one per line"));
    CHECK(has(r.out, "game   pressed 1\n") && has(r.out, "nnh: line 3: expect \"pressed\" matched at frame"));
    CHECK(game.received.size() >= 3 && game.received[0].type == nnh::detail::remote::MessageType::Hello);
}
