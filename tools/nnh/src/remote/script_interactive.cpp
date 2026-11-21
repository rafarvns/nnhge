// Script runner, interactive mode (see script_runner.hpp, spec 013 FR-5): commands typed in nnh's terminal,
// each run as soon as it is typed. Not repeatable by nature: a line starts at the last frame heard +
// kTypedDelay, but never before what was already sent (the console refuses frames out of order).

#include "remote/log_match.hpp"
#include "remote/script_runner.hpp"
#include "remote/script_tokens.hpp"

namespace nnh::tool::remote {
namespace {

const char* const kHelp[] = {
    "commands (one per line, run as soon as typed; B = A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT):",
    "  press B[+B...]   release B[+B...]   release all   hold B[+B...] N   tap B[+B...]   wait N",
    "  stick circle|cstick X Y (-1..1)   touch X Y   drag X Y N   untouch   screenshot NAME",
    "  expect \"TEXT\" N (report a matching log line within N frames)   fail-on \"TEXT\"",
    "  end (detach: the game gets its controls back)   help",
};

std::string trimmed(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

std::string quoted(const std::string& text) { return "\"" + text + "\""; }

} // namespace

void ScriptRunner::on_typed(const std::string& text) {
    if (!options_.interactive || closed_) return;
    ++typed_lines_;
    if (lower(trimmed(text)) == "help") {
        for (const char* line : kHelp) notes_.push_back(line);
        return;
    }
    Script typed;
    std::vector<std::string> errors;
    parse_script_line(parser_, text, typed_lines_, typed, errors);
    for (std::string& e : errors) notes_.push_back(std::move(e));
    for (const Step& step : typed.steps) {
        if (!hello_sent_) send_hello();
        if (attached_) typed_step(step);
        else waiting_.push_back(step);
    }
    pump_typed();
}

void ScriptRunner::typed_step(const Step& step) {
    const std::string why = missing_control(step);
    if (!why.empty()) {
        problem(step.line, why);
        return;
    }
    switch (step.command) {
    case Command::End:
        send_bye();
        hello_sent_ = false;
        typed_.clear();
        waiting_.clear();
        expects_.clear();
        fail_ons_.clear();
        parser_ = ScriptParser{.file = "typed"};
        notes_.push_back("detached: the game has its controls back; the next command attaches again");
        return;
    case Command::Expect:
        expects_.push_back(
            Watch{.pattern = step.text, .from = last_frame_, .to = last_frame_ + step.frames, .line = step.line});
        return;
    case Command::FailOn:
        fail_ons_.push_back(Watch{.pattern = step.text, .from = last_frame_, .to = 0, .line = step.line});
        return;
    default: break;
    }
    timeline_.add(step);
    Segment typed = timeline_.take_open();
    std::uint32_t base = last_frame_ + kTypedDelay;
    if (last_sent_frame_ > base) base = last_sent_frame_;
    if (!typed_.empty() && typed_.back().offset > base) base = typed_.back().offset;
    for (TimedEvent& e : typed.events) {
        e.offset += base; // now an absolute frame
        typed_.push_back(std::move(e));
    }
}

void ScriptRunner::typed_line(const std::string& line, std::uint32_t frame) {
    for (const Watch& w : fail_ons_) {
        if (frame >= w.from && log_matches(w.pattern, line)) {
            notes_.push_back("line " + std::to_string(w.line) + ": fail-on " + quoted(w.pattern) + " matched: " + line);
        }
    }
    for (std::size_t i = 0; i < expects_.size();) {
        const Watch& w = expects_[i];
        if (frame >= w.from && frame <= w.to && log_matches(w.pattern, line)) {
            notes_.push_back("line " + std::to_string(w.line) + ": expect " + quoted(w.pattern) + " matched at frame " +
                             std::to_string(frame));
            expects_.erase(expects_.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
}

void ScriptRunner::typed_tick() {
    for (std::size_t i = 0; i < expects_.size();) {
        const Watch& w = expects_[i];
        if (last_tick_ > w.to) {
            notes_.push_back("line " + std::to_string(w.line) + ": expect " + quoted(w.pattern) +
                             ": no matching line in frames " + std::to_string(w.from) + "-" + std::to_string(w.to));
            expects_.erase(expects_.begin() + static_cast<long>(i));
        } else {
            ++i;
        }
    }
    pump_typed();
}

void ScriptRunner::pump_typed() {
    if (!attached_) return;
    while (!typed_.empty() && unapplied_.size() < kMaxUnapplied) {
        TimedEvent e = std::move(typed_.front());
        typed_.pop_front();
        wire::Message m = e.message;
        m.frame = e.offset;
        send(m, e.line, e.text);
    }
}

} // namespace nnh::tool::remote
