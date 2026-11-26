// Script runner (see script_runner.hpp): what both modes share, and script mode. Interactive mode is in
// script_interactive.cpp.

#include "remote/script_runner.hpp"

#include <cstdio>

#include "remote/log_match.hpp"

namespace nnh::tool::remote {
namespace {

const char* reason_name(wire::Reject reason) {
    switch (reason) {
    case wire::Reject::Magic: return "bad magic";
    case wire::Reject::Type: return "unknown type";
    case wire::Reject::Range: return "value out of range";
    case wire::Reject::Order: return "frame out of order";
    case wire::Reject::NotAttached: return "no Hello yet";
    case wire::Reject::TooMany: return "remote input is off";
    default: return "unknown reason";
    }
}

std::string quoted(const std::string& text) { return "\"" + text + "\""; }

std::string frames(std::uint32_t from, std::uint32_t to) {
    return "frames " + std::to_string(from) + "-" + std::to_string(to);
}

/// The console's warning that one of its own controls was busy when the script attached (remote_session.cpp,
/// spec 013 amendment 2026-11-24): "W remote <what>: ignored while the script runs" → <what>; else "".
std::string control_not_at_rest(const std::string& line, const LineInfo& info) {
    static const std::string kChannel = " remote ", kSuffix = ": ignored while the script runs";
    if (!info.has_frame || info.level != 'W') return "";
    const std::size_t from = line.find(kChannel), to = line.rfind(kSuffix);
    if (from == std::string::npos || to == std::string::npos || to < from + kChannel.size()) return "";
    return line.substr(from + kChannel.size(), to - from - kChannel.size());
}

} // namespace

ScriptRunner::ScriptRunner(Script script, RunnerOptions options)
    : script_(std::move(script)), options_(options) {
    parser_.file = "typed";
    if (!options_.interactive) segments_ = build_timeline(script_);
}

void ScriptRunner::start() {
    if (!options_.interactive) send_hello();
}

void ScriptRunner::no_answer() {
    if (verdict_ == Verdict::Running && !attached_) {
        finish(Verdict::Error, 0, "the game didn't answer: a Release build, or one older than nnh");
    }
}

void ScriptRunner::on_closed() {
    if (closed_) return;
    if (options_.interactive) {
        closed_ = true;
        attached_ = false;
        finished_ = true;
        return;
    }
    closed_ = true; // no Bye on a closed connection
    if (verdict_ == Verdict::Running) {
        if (attached_) finish(Verdict::Fail, segments_[segment_].until.line, "the game closed before the script ended");
        else finish(Verdict::Error, 0, "the game closed before answering");
    }
    attached_ = false;
}

void ScriptRunner::on_line(const std::string& line) {
    if (closed_ || (!options_.interactive && verdict_ != Verdict::Running)) return;
    const LineInfo info = line_info(line);
    const std::uint32_t frame = info.has_frame ? info.frame : last_tick_;
    if (frame > last_frame_) last_frame_ = frame;
    if (options_.interactive) {
        dump_line(line);
        typed_line(line, frame);
        return;
    }
    dump_line(line);
    const std::string busy = control_not_at_rest(line, info);
    if (!busy.empty()) notes_.push_back("the console's own controls are ignored while the script runs: " + busy);
    script_line(line, frame, info.level);
}

void ScriptRunner::on_record(Record record) {
    if (closed_ || (!options_.interactive && verdict_ != Verdict::Running)) return;
    const auto line_of = [&](std::uint16_t seq) {
        const auto it = sent_.find(seq);
        return it == sent_.end() ? 0 : it->second.line;
    };
    switch (record.type) {
    case wire::RecordType::Hello:
        if (!hello_sent_) return;
        attached_ = true;
        if (record.frame > last_frame_) last_frame_ = record.frame;
        if (record.version != wire::kVersion) {
            finish(Verdict::Error, 0, "the game speaks remote protocol version " + std::to_string(record.version) +
                                          ", nnh speaks " + std::to_string(wire::kVersion) + ": rebuild one of them");
            return;
        }
        capabilities_ = record.capabilities;
        if (options_.interactive) {
            for (const Step& step : waiting_) typed_step(step);
            waiting_.clear();
            pump_typed();
        } else {
            hello_answered(record);
        }
        return;
    case wire::RecordType::Tick: tick(record.frame); return;
    case wire::RecordType::Late:
        problem(line_of(record.seq), "the input for frame " + std::to_string(record.wanted) +
                                         " reached the console on frame " + std::to_string(record.applied) +
                                         " (late): give it more frames, e.g. a longer wait before it");
        return;
    case wire::RecordType::Rejected:
        if (record.reason == wire::Reject::Type && waiting_catalogue_) { // spec 030's messages, to an older game
            waiting_catalogue_ = false;
            if (options_.interactive) notes_.push_back("the game has no live values (built before spec 030)");
            else finish(Verdict::Error, 0, "the game has no live values (built before spec 030): rebuild it");
            return;
        }
        problem(line_of(record.seq),
                std::string("the console rejected the message (") + reason_name(record.reason) + ")");
        return;
    case wire::RecordType::Dropped:
        problem(line_of(record.seq), "the console's queue was full (message dropped)");
        return;
    case wire::RecordType::Screen:
    case wire::RecordType::ScreenFailed: screen(record); return;
    case wire::RecordType::LiveEntry:
    case wire::RecordType::LiveRemoved:
    case wire::RecordType::LiveValue:
    case wire::RecordType::LiveRefused:
    case wire::RecordType::LiveState: on_live_record(record); return;
    case wire::RecordType::ReloadStaged:
    case wire::RecordType::ReloadApplied:
    case wire::RecordType::ReloadRefused: return; // spec 031: the reload controller's (stage 3)
    }
}

std::vector<MessageBytes> ScriptRunner::take_messages() { return std::move(messages_); }
std::vector<std::string> ScriptRunner::take_notes() { return std::move(notes_); }
std::vector<ScreenJob> ScriptRunner::take_screens() { return std::move(screens_); }

int ScriptRunner::exit_code() const {
    if (options_.interactive) return 0;
    switch (verdict_) {
    case Verdict::Pass: return 0;
    case Verdict::Fail: return 1;
    default: return 2;
    }
}

// ---- shared ---------------------------------------------------------------------------------------------

void ScriptRunner::send(wire::Message message, int line, const std::string& name) {
    message.seq = ++seq_;
    Sent sent{.line = line, .name = name, .screens_left = 0};
    if (message.type == wire::MessageType::Screenshot) {
        message.id = message.seq;
        sent.screens_left = ((message.screens & wire::kScreenTop) ? 1 : 0) +
                            ((message.screens & wire::kScreenBottom) ? 1 : 0) +
                            ((message.screens & wire::kScreenTopRight) ? 1 : 0);
        ++screens_pending_;
    }
    sent_[message.seq] = sent;
    MessageBytes bytes;
    wire::encode_message(message, bytes.data());
    messages_.push_back(bytes);
    unapplied_.push_back(message.frame);
    if (message.frame > last_sent_frame_) last_sent_frame_ = message.frame;
}

void ScriptRunner::send_hello() {
    wire::Message hello;
    hello.type = wire::MessageType::Hello;
    hello.version = wire::kVersion;
    hello.exclusive = !options_.interactive; // a script owns the controls; a person typing shares them
    MessageBytes bytes;
    wire::encode_message(hello, bytes.data());
    messages_.push_back(bytes);
    hello_sent_ = true;
}

void ScriptRunner::send_bye() {
    wire::Message bye;
    bye.type = wire::MessageType::Bye;
    bye.quit = options_.quit_at_end;
    MessageBytes bytes;
    wire::encode_message(bye, bytes.data());
    messages_.push_back(bytes);
    attached_ = false;
    unapplied_.clear();
}

void ScriptRunner::tick(std::uint32_t frame) {
    if (frame > last_tick_) last_tick_ = frame;
    if (frame > last_frame_) last_frame_ = frame;
    while (!unapplied_.empty() && unapplied_.front() <= last_tick_) unapplied_.pop_front();
    if (options_.interactive) typed_tick();
    else script_tick();
}

void ScriptRunner::screen(Record& record) {
    const auto it = sent_.find(record.screen.id);
    if (it == sent_.end() || it->second.screens_left == 0) return; // not ours, or already complete
    Sent& sent = it->second;
    if (--sent.screens_left == 0) --screens_pending_;
    if (record.type == wire::RecordType::ScreenFailed) {
        problem(sent.line, "the console couldn't take screenshot " + sent.name + " (frame " +
                               std::to_string(record.frame) + ")");
        return;
    }
    ScreenJob job;
    const std::string error = screen_to_rgb(record.screen, record.pixels, job.image);
    if (!error.empty()) {
        problem(sent.line, "screenshot " + sent.name + ": " + error);
        return;
    }
    char file[96];
    static constexpr const char* kSuffix[3] = {"top", "bottom", "top-right"};
    std::snprintf(file, sizeof file, "frame-%06u-%s-%s.png", static_cast<unsigned>(record.frame), sent.name.c_str(),
                  kSuffix[record.screen.screen < 3 ? record.screen.screen : 0]);
    job.line = sent.line;
    job.name = sent.name;
    job.file_name = file;
    screens_.push_back(std::move(job));
    if (!options_.interactive) check_end();
}

void ScriptRunner::finish(Verdict verdict, int line, const std::string& why) {
    if (verdict_ != Verdict::Running) return;
    verdict_ = verdict;
    finished_ = true;
    if (verdict == Verdict::Pass) {
        notes_.push_back("PASS (" + std::to_string(script_.lines) + " lines, " + frames(1, final_frame_) + ")");
    } else if (verdict == Verdict::Fail) {
        notes_.push_back("FAIL at " + where(line) + ": " + why);
    } else {
        notes_.push_back(why);
    }
    if (attached_ && !closed_) send_bye();
}

void ScriptRunner::problem(int line, const std::string& why) {
    if (!options_.interactive) finish(Verdict::Fail, line, why);
    else if (line > 0) notes_.push_back("line " + std::to_string(line) + ": " + why);
    else notes_.push_back(why);
}

std::string ScriptRunner::where(int line) const { return script_.file + ":" + std::to_string(line); }

std::string ScriptRunner::missing_control(const Step& step) const {
    const bool buttons = step.command == Command::Press || step.command == Command::Release ||
                         step.command == Command::Hold || step.command == Command::Tap;
    if (buttons && (step.buttons & (kButtonZL | kButtonZR)) != 0 && (capabilities_ & kCapabilityZButtons) == 0) {
        return "this console has no ZL/ZR (an Old 3DS?)";
    }
    if (step.command == Command::Stick && step.stick == 1 && (capabilities_ & kCapabilityCStick) == 0) {
        return "this console has no C-Stick (an Old 3DS?)";
    }
    return "";
}

// ---- script mode ----------------------------------------------------------------------------------------

void ScriptRunner::hello_answered(const Record&) {
    for (const Step& step : script_.steps) {
        const std::string why = missing_control(step);
        if (!why.empty()) {
            finish(Verdict::Fail, step.line, why);
            return;
        }
    }
    if (script_uses_live()) { // spec 030: the catalogue first; the script starts when it is complete
        request_catalogue();
        return;
    }
    segment_ = 0;
    begin_segment(1);
}

void ScriptRunner::begin_segment(std::uint32_t base) {
    base_ = base;
    next_event_ = 0;
    expect_value_sent_ = false;
    const Segment& seg = segments_[segment_];
    if (seg.until.command == Command::End) final_frame_ = base + seg.length;
    pump();
    check_end();
}

void ScriptRunner::pump(bool ignore_room) {
    const Segment& seg = segments_[segment_];
    while (next_event_ < seg.events.size() && (ignore_room || unapplied_.size() < kMaxUnapplied)) {
        const TimedEvent& e = seg.events[next_event_++];
        const std::uint32_t frame = base_ + e.offset;
        if (e.kind == TimedEvent::Kind::FailOn) {
            fail_ons_.push_back(Watch{.pattern = e.text, .from = frame, .to = 0, .line = e.line});
        } else {
            wire::Message m = e.message;
            m.frame = frame;
            if (!prepare_live(e, m)) {
                if (verdict_ != Verdict::Running) return;
                continue; // a `--tunes` value the game doesn't declare: skipped
            }
            send(m, e.line, e.text);
        }
    }
    if (next_event_ == seg.events.size() && seg.until.command == Command::ExpectValue && !expect_value_sent_ &&
        verdict_ == Verdict::Running) {
        expect_value_sent_ = true;
        start_expect_value(seg.until, base_ + seg.length);
    }
}

void ScriptRunner::check_end() {
    if (verdict_ != Verdict::Running || !attached_) return;
    const Segment& seg = segments_[segment_];
    if (seg.until.command != Command::End || next_event_ < seg.events.size()) return;
    if (last_tick_ >= final_frame_ && screens_pending_ == 0) finish(Verdict::Pass, 0, "");
}

void ScriptRunner::script_line(const std::string& line, std::uint32_t frame, char level) {
    if (!attached_) return;
    const Segment& seg = segments_[segment_];
    const bool expected = seg.until.command == Command::Expect && log_matches(seg.until.text, line);
    if (level == 'F' && !expected) { // a crash test expects its fatal line; any other fatal line fails
        finish(Verdict::Fail, seg.until.line, "the game logged a fatal error: " + line);
        return;
    }
    for (const Watch& w : fail_ons_) {
        if (frame >= w.from && log_matches(w.pattern, line)) {
            finish(Verdict::Fail, w.line, "fail-on " + quoted(w.pattern) + " matched: " + line);
            return;
        }
    }
    const std::uint32_t to = base_ + seg.length + seg.until.frames;
    if (seg.until.command == Command::Expect && frame >= base_ && frame <= to &&
        log_matches(seg.until.text, line)) {
        notes_.push_back("line " + std::to_string(seg.until.line) + ": expect " + quoted(seg.until.text) +
                         " matched at frame " + std::to_string(frame));
        pump(true); // anything of this segment not sent yet goes now (the console reports it late)
        ++segment_;
        begin_segment(frame + 1);
        if (level == 'F') { // the game stopped: it passes only when nothing of the script is left
            const Segment& next = segments_[segment_];
            if (next.until.command == Command::End && next.events.empty() && next.length == 0) {
                finish(Verdict::Pass, 0, "");
            } else {
                finish(Verdict::Fail, next.until.line,
                       "the game stopped at a fatal error before the script's end: " + line);
            }
        }
    }
}

void ScriptRunner::script_tick() {
    if (!attached_ || verdict_ != Verdict::Running || waiting_catalogue_) return; // spec 030: the script starts after it
    const Segment& seg = segments_[segment_];
    const std::uint32_t to = base_ + seg.length + seg.until.frames;
    if (seg.until.command == Command::Expect && last_tick_ > to) {
        finish(Verdict::Fail, seg.until.line,
               "expect " + quoted(seg.until.text) + ": no matching line in " + frames(base_, to));
        return;
    }
    if (value_watch_.active && !paused_ && last_tick_ > value_watch_.to + 30) {
        finish(Verdict::Fail, value_watch_.line,
               value_watch_.text + ": no value in " + frames(value_watch_.from, value_watch_.to));
        return;
    }
    pump();
    check_end();
}

} // namespace nnh::tool::remote
