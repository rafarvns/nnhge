// The script runner's live values (spec 030 FR-11, FR-12, FR-14, FR-16; plan "Script commands", "nnh commands"): the
// catalogue requested at the Hello answer when the script or a typed line needs it, names and values resolved when
// their message is sent (a name a scene registers later works), expect-value through follows and LiveValue records,
// LiveState as the console's progress while frames stop (pauses), refusals failing with their line, dumps captured
// from the log for the session to save.

#include <algorithm>
#include <charconv>

#include "remote/live_text.hpp"
#include "remote/script_live.hpp"
#include "remote/script_runner.hpp"

namespace nnh::tool::remote {
namespace {

const char* const kOps[] = {"==", "!=", "<", "<=", ">", ">="};

} // namespace

bool ScriptRunner::script_uses_live() const {
    for (const Step& s : script_.steps) {
        if (is_live_command(s.command)) return true;
    }
    return false;
}

void ScriptRunner::request_catalogue() {
    catalog_.clear();
    send(wire::live_message(wire::LiveOp::Catalogue, 0, 0, 0), 0, "");
    waiting_catalogue_ = true;
}

bool ScriptRunner::prepare_live(const TimedEvent& e, wire::Message& m) {
    if (!e.live) return true;
    const std::uint32_t at = m.frame;
    if (e.live_command == Command::Reset) {
        if (e.text == "all") return true;
        const bool group = catalog_.find(e.text) == nullptr && catalog_.is_group(e.text);
        if (!group && catalog_.find(e.text) == nullptr) {
            problem(e.line, e.text + " is not declared by the game (frame " + std::to_string(at) + "): " +
                                unknown_name(catalog_, e.text));
            return false;
        }
        m.stick = static_cast<std::uint8_t>(group ? wire::ResetScope::Group : wire::ResetScope::One);
        return true;
    }
    if (e.live_command != Command::Set && e.live_command != Command::Action) return true;
    const LiveItem* item = catalog_.find(e.text);
    if (item == nullptr && !e.origin.empty()) { // --tunes: reported and skipped
        notes_.push_back(e.origin + ": unknown name \"" + e.text + "\": skipped");
        return false;
    }
    if (item == nullptr) {
        problem(e.line, e.text + " is not declared by the game (frame " + std::to_string(at) + "): " +
                            unknown_name(catalog_, e.text));
        return false;
    }
    if (e.live_command == Command::Action) {
        if (item->action()) return true;
        problem(e.line, e.text + " is not an action");
        return false;
    }
    std::uint32_t bits = 0;
    const std::string why = parse_set_value(*item, e.value, bits);
    if (!why.empty()) {
        problem(e.line, e.origin.empty() ? why : e.origin + ": " + why);
        return false;
    }
    m.release = bits;
    return true;
}

void ScriptRunner::start_expect_value(const Step& step, std::uint32_t from) {
    ValueWatch& w = value_watch_;
    w = ValueWatch{};
    const LiveItem* item = catalog_.find(step.text);
    if (item == nullptr) {
        problem(step.line, step.text + " is not declared by the game (frame " + std::to_string(from) +
                               "): " + unknown_name(catalog_, step.text));
        return;
    }
    const std::string why = parse_expected(*item, step.value, static_cast<CompareOp>(step.compare), w.expected);
    if (!why.empty()) {
        problem(step.line, why);
        return;
    }
    w.active = true;
    w.line = step.line;
    w.hash = item->hash;
    w.from = from;
    w.to = from + step.frames;
    w.within = step.frames > 0;
    w.op = static_cast<CompareOp>(step.compare);
    w.tolerance = step.tolerance;
    w.text = "expect-value " + step.text + " " + kOps[step.compare] + " " + step.value;
    wire::Message m = wire::live_message(wire::LiveOp::Follow, 1,
                                         static_cast<std::uint16_t>(w.within ? step.frames + 1 : 1), item->hash);
    m.frame = from;
    send(m, step.line, "");
}

void ScriptRunner::live_value(const Record& record) {
    wire::LiveValueRecord v;
    if (!wire::decode_live_value(record.payload.data(), static_cast<std::uint32_t>(record.payload.size()), v)) return;
    ValueWatch& w = value_watch_;
    if (options_.interactive && w.active && v.hash == w.hash) { // a typed expect-value: reported, never ends the run
        const LiveItem* item = catalog_.find(v.hash);
        if (value_passes(v, w.op, w.expected, w.tolerance)) {
            notes_.push_back("line " + std::to_string(w.line) + ": " + w.text + " passed at frame " +
                             std::to_string(v.frame) + " (" + value_text(v, item) + ")");
            w.active = false;
        } else if (!w.within || v.frame >= w.to) {
            notes_.push_back("line " + std::to_string(w.line) + ": " + w.text + ": the value is " + value_text(v, item) +
                             " at frame " + std::to_string(v.frame));
            w.active = false;
        }
        return;
    }
    if (options_.interactive) {
        const LiveItem* item = catalog_.find(v.hash);
        notes_.push_back("[" + std::to_string(v.frame) + "] " + (item != nullptr ? item->name : "?") + " = " +
                         value_text(v, item) + ((v.flags & wire::kValuePaused) != 0 ? " (paused)" : ""));
        return;
    }
    if (!w.active || v.hash != w.hash || verdict_ != Verdict::Running) return;
    const bool paused = (v.flags & wire::kValuePaused) != 0;
    if (w.within && !paused && v.frame < w.from) return;
    const LiveItem* item = catalog_.find(v.hash);
    if (value_passes(v, w.op, w.expected, w.tolerance)) {
        notes_.push_back("line " + std::to_string(w.line) + ": " + w.text + " passed at frame " + std::to_string(v.frame) +
                         " (" + value_text(v, item) + ")");
        w.active = false;
        if (w.within) {
            wire::Message un = wire::live_message(wire::LiveOp::Unfollow, 0, 0, w.hash);
            send(un, w.line, "");
        }
        ++segment_;
        begin_segment(paused ? v.frame + 1 : v.frame + 1);
        return;
    }
    if (!w.within || v.frame >= w.to) {
        finish(Verdict::Fail, w.line, w.text + ": the value is " + value_text(v, item) + " at frame " +
                                          std::to_string(v.frame));
    }
}

void ScriptRunner::live_state(const Record& record) {
    wire::LiveStateRecord s;
    if (!wire::decode_live_state(record.payload.data(), static_cast<std::uint32_t>(record.payload.size()), s)) return;
    paused_ = s.paused;
    // Messages due at s.frame were applied (a paused tick, or the frame's start): progress while ticks stop.
    const std::uint32_t applied = s.paused ? s.frame : s.frame - 1;
    if (applied > last_tick_) {
        last_tick_ = applied;
        while (!unapplied_.empty() && unapplied_.front() <= last_tick_) unapplied_.pop_front();
    }
    if ((s.flags & wire::kStateCatalogueComplete) != 0 && waiting_catalogue_) {
        waiting_catalogue_ = false;
        catalog_.complete = true;
        if (options_.interactive) {
            pump_typed();
        } else if (verdict_ == Verdict::Running) {
            segment_ = 0;
            begin_segment(s.paused ? s.frame : s.frame + 2);
        }
        return;
    }
    if (options_.interactive) pump_typed();
    else if (verdict_ == Verdict::Running && !waiting_catalogue_ && !value_watch_.active) {
        pump();
        check_end();
    }
}

void ScriptRunner::on_live_record(const Record& record) {
    const auto line_of = [&](std::uint16_t seq) {
        const auto it = sent_.find(seq);
        return it == sent_.end() ? 0 : it->second.line;
    };
    switch (record.type) {
    case wire::RecordType::LiveEntry:
        catalog_.add_entry(record.payload.data(), static_cast<std::uint32_t>(record.payload.size()));
        return;
    case wire::RecordType::LiveRemoved: {
        std::uint32_t frame = 0, hash = 0;
        if (wire::decode_live_removed(record.payload.data(), 8, frame, hash)) catalog_.remove(hash);
        return;
    }
    case wire::RecordType::LiveValue: live_value(record); return;
    case wire::RecordType::LiveState: live_state(record); return;
    case wire::RecordType::LiveRefused: {
        wire::LiveRefusedRecord r;
        if (!wire::decode_live_refused(record.payload.data(), 8, r)) return;
        const LiveItem* item = catalog_.find(r.hash);
        problem(line_of(r.seq), std::string("the game refused it (") + detail::live::refusal_text(r.reason) + ")" +
                                    (item != nullptr ? ": " + item->name : ""));
        return;
    }
    default: return;
    }
}

void ScriptRunner::dump_line(const std::string& line) {
    const std::size_t at = line.find("dump: ");
    if (at == std::string::npos) return;
    const std::string text = line.substr(at + 6);
    if (text.rfind("begin", 0) == 0) {
        in_dump_ = true;
        dump_lines_.clear();
        dump_lines_.push_back("# " + text);
        return;
    }
    if (!in_dump_) return;
    if (text == "end") {
        in_dump_ = false;
        std::string file;
        for (const std::string& l : dump_lines_) file += l + "\n";
        dumps_.push_back(std::move(file));
        return;
    }
    dump_lines_.push_back(text);
}

std::vector<std::string> ScriptRunner::take_dumps() { return std::move(dumps_); }

bool ScriptRunner::typed_live(const std::string& text) {
    std::vector<Token> t;
    if (!tokenize(text, t).empty() || t.empty()) return false;
    const std::string word = lower(t[0].text);
    if (word != "tunes" && word != "watches" && word != "get" && word != "watch" && word != "unwatch") return false;
    if (!hello_sent_) send_hello();
    if (!catalog_.complete && !waiting_catalogue_ && attached_) request_catalogue();
    if (word == "tunes" || word == "watches") {
        for (const LiveItem* i : catalog_.list(t.size() > 1 ? t[1].text : "")) {
            if ((word == "tunes") != (i->tunable() || i->action())) continue;
            std::string line = "  " + i->name + "  ";
            if (i->tunable()) {
                line += bits_text(*i, i->value) + " (default " + bits_text(*i, i->def) + ")";
                if (i->kind == detail::live::Kind::Float || i->kind == detail::live::Kind::Int32) {
                    line += " " + bits_text(*i, i->min) + ".." + bits_text(*i, i->max) + " step " + bits_text(*i, i->step);
                }
                if (i->value != i->def) line += " *";
            } else {
                line += i->action() ? "action" : "watch";
            }
            notes_.push_back(line);
        }
        if (!catalog_.complete) notes_.push_back("(the catalogue is still on its way: try again)");
        return true;
    }
    if (t.size() < 2) {
        notes_.push_back("usage: " + word + " NAME");
        return true;
    }
    const LiveItem* item = catalog_.find(t[1].text);
    if (item == nullptr && !(word == "unwatch" && t[1].text == "all")) {
        notes_.push_back(unknown_name(catalog_, t[1].text));
        return true;
    }
    const std::uint32_t hash = item != nullptr ? item->hash : 0;
    std::uint8_t every = 1;
    if (word == "watch" && t.size() != 2) {
        int n = 0;
        const std::string& v = t.size() == 4 ? t[3].text : std::string();
        const auto r = std::from_chars(v.data(), v.data() + v.size(), n);
        if (t.size() != 4 || t[2].text != "every" || r.ec != std::errc{} || r.ptr != v.data() + v.size() || n < 1 ||
            n > 255) {
            notes_.push_back("usage: watch NAME [every N] (N 1..255)");
            return true;
        }
        every = static_cast<std::uint8_t>(n);
    }
    const wire::LiveOp op = word == "get" ? wire::LiveOp::Get : (word == "watch" ? wire::LiveOp::Follow : wire::LiveOp::Unfollow);
    wire::Message m = wire::live_message(op, op == wire::LiveOp::Follow ? every : 0, 0, hash);
    m.frame = std::max(last_sent_frame_, last_frame_ + kTypedDelay);
    send(m, typed_lines_, "");
    return true;
}

} // namespace nnh::tool::remote
