#pragma once
// Runs a test script, or lines typed one by one, against a connected Development game (spec 013 FR-3, FR-5;
// plan "Runner"). Pure: the session (stage 4) feeds it what the game sent — records and log lines — and
// sends the messages it hands out, writes the screenshots and prints the notes. Frame-driven only: wall-clock
// time (the wait for the Hello answer) is the session's, which reports it with no_answer().
// Tested against a fake console in tools/nnh/tests/test_script_runner.cpp and test_script_interactive.cpp.
//
// Script mode: Hello first, flagged exclusive (the game then reads only the script's input; the console's warning
// about its own controls not at rest becomes a note — spec 013 amendment 2026-11-24); at the answer, every
// control the script uses must exist (ZL/ZR, C-Stick); then messages go out in frame order, at most kMaxUnapplied ahead of the console's last tick. An `expect` is a
// barrier: nothing after it is sent before a matching line arrives; its window runs from the segment's first
// frame (frame 1, or the frame after the previous expect's line) to N frames after the clock at the expect,
// and it times out at the first tick past that window. After a match the clock = the line's frame + 1.
// `end` passes once a tick has reached the final clock and every screenshot is in. Failures: timeout,
// `fail-on`, a level-F line, Late/Rejected/Dropped/ScreenFailed, a bad screen, the game closing first.
//
// Interactive mode: nothing is sent before the first typed command (then Hello, not exclusive: real and remote
// input are merged); each line starts at the last frame heard + kTypedDelay (never before what was already
// sent); `expect` reports and never ends the run; `end` sends Bye and detaches (the next command attaches again); `help` lists the commands.

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "remote/game_records.hpp"
#include "remote/screen_image.hpp"
#include "remote/script.hpp"
#include "remote/script_timeline.hpp"

namespace nnh::tool::remote {

/// Messages sent but not yet applied (frame after the last tick); the console's queue holds 64.
inline constexpr std::size_t kMaxUnapplied = 48;
/// Typed commands start this many frames after the last frame heard (~100 ms at 60 Hz).
inline constexpr std::uint32_t kTypedDelay = 6;

struct RunnerOptions {
    bool interactive = false;
    bool quit_at_end = false; ///< the Bye at the end asks the game to quit (scripts, unless --keep-open)
};

enum class Verdict : std::uint8_t { Running, Pass, Fail, Error };

/// A screenshot to save, already converted.
struct ScreenJob {
    int line = 0;
    std::string name;
    std::string file_name; ///< "frame-000061-NAME-top.png"
    RgbImage image;
};

using MessageBytes = std::array<std::uint8_t, wire::kMessageSize>;

class ScriptRunner {
public:
    /// Script mode: `script` parsed without errors. Interactive mode: pass an empty Script.
    ScriptRunner(Script script, RunnerOptions options);

    /// The game connected. Script mode sends Hello; interactive mode waits for the first command.
    void start();
    /// The game didn't answer the Hello in time (Error).
    void no_answer();
    void on_line(const std::string& line);
    void on_record(Record record);
    /// The game closed the connection.
    void on_closed();
    /// Interactive mode: one typed line.
    void on_typed(const std::string& text);

    /// What to send, in order (16-byte messages).
    std::vector<MessageBytes> take_messages();
    /// Progress and result lines, without the "nnh: " prefix.
    std::vector<std::string> take_notes();
    std::vector<ScreenJob> take_screens();

    Verdict verdict() const { return verdict_; }
    /// Script mode: a verdict was reached. Interactive mode: the game closed.
    bool finished() const { return finished_; }
    /// 0 pass, 1 fail, 2 could not run; interactive mode: 0.
    int exit_code() const;
    /// True while a Hello was answered and no Bye was sent (the session may wait for the Hello answer).
    bool attached() const { return attached_; }
    bool hello_sent() const { return hello_sent_; }
    bool interactive() const { return options_.interactive; }

private:
    struct Sent {
        int line = 0;
        std::string name; ///< Screenshot NAME
        int screens_left = 0;
    };
    struct Watch {
        std::string pattern;
        std::uint32_t from = 0;
        std::uint32_t to = 0; ///< expect only
        int line = 0;
    };

    // shared (script_runner.cpp)
    void send(wire::Message message, int line, const std::string& name);
    void send_hello();
    void send_bye();
    void tick(std::uint32_t frame);
    void screen(Record& record);
    void finish(Verdict verdict, int line, const std::string& why);
    void problem(int line, const std::string& why);
    std::string where(int line) const;
    std::string missing_control(const Step& step) const;
    // script mode
    void hello_answered(const Record& hello);
    void begin_segment(std::uint32_t base);
    void pump(bool ignore_room = false);
    void check_end();
    void script_line(const std::string& line, std::uint32_t frame, char level);
    void script_tick();
    // interactive mode (script_interactive.cpp)
    void typed_step(const Step& step);
    void typed_line(const std::string& line, std::uint32_t frame);
    void typed_tick();
    void pump_typed();

    Script script_;
    RunnerOptions options_;
    std::vector<Segment> segments_;
    std::size_t segment_ = 0;
    std::size_t next_event_ = 0;
    std::uint32_t base_ = 1;
    std::uint32_t final_frame_ = 0;

    Verdict verdict_ = Verdict::Running;
    bool finished_ = false;
    bool hello_sent_ = false;
    bool attached_ = false;
    bool closed_ = false;
    std::uint32_t capabilities_ = 0;
    std::uint32_t last_tick_ = 0;
    std::uint32_t last_frame_ = 0; ///< the latest frame heard (ticks and lines)
    std::uint32_t last_sent_frame_ = 0;
    std::uint16_t seq_ = 0;
    std::deque<std::uint32_t> unapplied_; ///< frames of messages after the last tick, ascending
    std::unordered_map<std::uint16_t, Sent> sent_;
    std::vector<Watch> fail_ons_;
    std::vector<Watch> expects_; ///< interactive: every pending expect
    int screens_pending_ = 0;    ///< screenshot messages whose two screens aren't all in

    // interactive
    ScriptParser parser_;
    Timeline timeline_;
    std::deque<TimedEvent> typed_; ///< offset = absolute frame
    std::vector<Step> waiting_;    ///< typed before the Hello was answered
    int typed_lines_ = 0;

    std::vector<MessageBytes> messages_;
    std::vector<std::string> notes_;
    std::vector<ScreenJob> screens_;
};

} // namespace nnh::tool::remote
