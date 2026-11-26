// Live value lines in scripts (spec 030 T6.2, T6.3, T6.4, T6.5): syntax and errors with their lines, the paused-section
// rules, the timeline's events and clock, and the runner against a fake console (the catalogue first, names resolved
// when sent, expect-value passing and failing, refusals failing with their line, an older game, dumps captured,
// interactive tunes/get and expect-value).

#include <cstring>
#include <string>
#include <vector>

#include "live/live_kinds.hpp"
#include "nnh/detail/name_hash.hpp"
#include "remote/live_wire.hpp"
#include "remote/script.hpp"
#include "remote/script_runner.hpp"
#include "remote/script_timeline.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
namespace lv = nnh::detail::live;

namespace {

ParsedScript parse(const char* text) { return parse_script(text, "t.nnhs"); }

std::vector<wire::Message> decoded(ScriptRunner& r) {
    std::vector<wire::Message> out;
    for (const MessageBytes& b : r.take_messages()) {
        wire::Message m;
        CHECK(wire::decode_message(b.data(), m) == wire::Reject::None);
        out.push_back(m);
    }
    return out;
}

Record hello_record(std::uint32_t frame) {
    Record r;
    r.type = wire::RecordType::Hello;
    r.version = wire::kVersion;
    r.frame = frame;
    return r;
}

Record payload_record(wire::RecordType type, const std::uint8_t* bytes, std::size_t n) {
    Record r;
    r.type = type;
    r.payload.assign(bytes + wire::kRecordHeaderSize, bytes + n);
    return r;
}

Record entry_record(const char* name, lv::Kind kind, std::uint32_t min, std::uint32_t max) {
    wire::LiveEntryHeader h;
    h.hash = nnh::detail::fnv1a32(name);
    h.kind = kind;
    h.flags = wire::kEntryTunable;
    h.min = min;
    h.max = max;
    h.step = lv::float_bits(0.25f);
    std::uint8_t buf[wire::kRecordHeaderSize + wire::kLiveEntryMax];
    return payload_record(wire::RecordType::LiveEntry, buf, wire::encode_live_entry(h, name, nullptr, 0, buf));
}

Record state_record(std::uint32_t frame, bool paused, std::uint8_t flags) {
    std::uint8_t buf[16];
    return payload_record(wire::RecordType::LiveState, buf, wire::encode_live_state(frame, paused, flags, 0, buf));
}

Record value_record(const char* name, std::uint32_t frame, float v) {
    wire::LiveValueRecord rec;
    rec.frame = frame;
    rec.hash = nnh::detail::fnv1a32(name);
    rec.kind = lv::Kind::Float;
    rec.length = 4;
    const std::uint32_t bits = lv::float_bits(v);
    std::memcpy(rec.value, &bits, 4);
    std::uint8_t buf[wire::kRecordHeaderSize + wire::kLiveValueMax];
    return payload_record(wire::RecordType::LiveValue, buf, wire::encode_live_value(rec, buf));
}

Record tick(std::uint32_t frame) {
    Record r;
    r.type = wire::RecordType::Tick;
    r.frame = frame;
    return r;
}

bool has_note(ScriptRunner& r, const std::string& part, std::vector<std::string>& notes) {
    for (std::string& n : r.take_notes()) notes.push_back(std::move(n));
    for (const std::string& n : notes) {
        if (n.find(part) != std::string::npos) return true;
    }
    return false;
}

/// A runner attached to a console with player/jump (0..10) in its catalogue, the script started at frame 10.
ScriptRunner started(const char* text, std::vector<wire::Message>& sent) {
    ParsedScript p = parse(text);
    CHECK(p.errors.empty());
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.start();
    r.on_record(hello_record(1));
    sent = decoded(r); // Hello, the catalogue request
    r.on_record(entry_record("player/jump", lv::Kind::Float, lv::float_bits(0.0f), lv::float_bits(10.0f)));
    r.on_record(state_record(8, false, wire::kStateCatalogueComplete));
    for (const wire::Message& m : decoded(r)) sent.push_back(m);
    return r;
}

} // namespace

NNH_TEST(script_live_parses_every_command) {
    const ParsedScript p = parse("set player/jump 5.25\nset colors/sky \"#FFC040\"\nreset player\nreset all\n"
                                 "action player/respawn\ndump\npause\nstep\nstep 3\nresume\n"
                                 "expect-value player/jump == 6.25\nexpect-value player/pos == \"(1, 2)\" ~ 0.5 within 30\n");
    CHECK(p.errors.empty());
    const std::vector<Step>& s = p.script.steps;
    CHECK(s[0].command == Command::Set && s[0].text == "player/jump" && s[0].value == "5.25");
    CHECK(s[1].value == "#FFC040");
    CHECK(s[2].command == Command::Reset && s[3].text == "all");
    CHECK(s[4].command == Command::Action && s[5].command == Command::Dump && s[6].command == Command::Pause);
    CHECK(s[7].command == Command::StepFrames && s[7].frames == 1 && s[8].frames == 3);
    CHECK(s[10].command == Command::ExpectValue && s[10].compare == 0 && s[10].frames == 0);
    CHECK(s[11].tolerance == 0.5 && s[11].frames == 30 && s[11].value == "(1, 2)");
}

NNH_TEST(script_live_errors_name_their_lines) {
    const ParsedScript p = parse("set player.jump 1\nset a\nexpect-value a ~= 1\nstep 3601\nexpect-value a == 1 within x\n");
    CHECK(p.errors.size() == 5);
    CHECK(p.errors[0] == "t.nnhs:1: name \"player.jump\" has a character other than letters, digits, '_', '-' and '/'");
    CHECK(p.errors[1] == "t.nnhs:2: wrong number of arguments; usage: set NAME VALUE");
    CHECK(p.errors[2] == "t.nnhs:3: OP is one of == != < <= > >=");
    CHECK(p.errors[3] == "t.nnhs:4: step is 1 to 3600 frames");
    CHECK(p.errors[4] == "t.nnhs:5: frames must be a whole number 1 to 216000");
}

NNH_TEST(script_live_paused_sections_refuse_clock_moves) {
    const ParsedScript p = parse("pause\ntap A\nwait 2\nhold A 3\nexpect \"x\" 5\nexpect-value a == 1 within 3\n"
                                 "touch 1 1\ndrag 2 2 4\nset a 1\nscreenshot s\nexpect-value a == 1\nstep 2\ntap A\nresume\ntap A\n");
    CHECK(p.errors.size() == 7);
    CHECK(p.errors[0] == "t.nnhs:2: tap while paused: step or resume first");
    CHECK(p.errors[1] == "t.nnhs:3: wait while paused: step or resume first");
    CHECK(p.errors[3] == "t.nnhs:5: expect while paused: step or resume first");
    CHECK(p.errors[4] == "t.nnhs:6: expect-value ... within while paused: step or resume first");
    CHECK(p.errors[5] == "t.nnhs:8: drag while paused: step or resume first");
    CHECK(p.errors[2] == "t.nnhs:4: hold while paused: step or resume first");
    CHECK(p.errors[6] == "t.nnhs:13: tap while paused: step or resume first");
}

NNH_TEST(script_live_timeline_clock) {
    const ParsedScript p = parse("set a 1\nwait 5\npause\nscreenshot s\nstep 3\nresume\nexpect-value a == 1\nend\n");
    const std::vector<Segment> segs = build_timeline(p.script);
    CHECK(segs.size() == 2);
    const Segment& s = segs[0];
    CHECK(s.events.size() == 5);
    CHECK(s.events[0].live && s.events[0].offset == 0 && s.events[0].message.type == wire::MessageType::LiveSet);
    CHECK(s.events[1].offset == 5 && s.events[1].message.type == wire::MessageType::Pause);
    CHECK(s.events[2].offset == 5 && s.events[2].message.type == wire::MessageType::Screenshot); // paused: no frame
    CHECK(s.events[3].offset == 5 && wire::pause_op(s.events[3].message) == wire::PauseOp::Step);
    CHECK(s.events[4].offset == 8 && wire::pause_op(s.events[4].message) == wire::PauseOp::Resume);
    CHECK(s.length == 8 && s.until.command == Command::ExpectValue);
}

NNH_TEST(script_runner_live_sets_after_the_catalogue_and_passes_expect_value) {
    std::vector<wire::Message> sent;
    ScriptRunner r = started("set player/jump 6.25\nexpect-value player/jump == 6.25\nend\n", sent);
    CHECK(sent.size() == 4); // Hello, Catalogue, LiveSet, Follow
    CHECK(sent[1].type == wire::MessageType::Live && wire::live_op(sent[1]) == wire::LiveOp::Catalogue);
    CHECK(sent[2].type == wire::MessageType::LiveSet && sent[2].frame == 10 &&
          wire::live_bits(sent[2]) == lv::float_bits(6.25f));
    CHECK(wire::live_op(sent[3]) == wire::LiveOp::Follow && sent[3].frame == 10);
    r.on_record(value_record("player/jump", 10, 6.25f));
    r.on_record(tick(12));
    std::vector<std::string> notes;
    CHECK(has_note(r, "line 2: expect-value player/jump == 6.25 passed at frame 10 (6.25)", notes));
    CHECK(r.verdict() == Verdict::Pass);
}

NNH_TEST(script_runner_live_fails_a_wrong_value_and_an_unknown_name) {
    std::vector<wire::Message> sent;
    ScriptRunner r = started("expect-value player/jump == 7\n", sent);
    r.on_record(value_record("player/jump", 10, 5.5f));
    std::vector<std::string> notes;
    CHECK(has_note(r, "FAIL at t.nnhs:1: expect-value player/jump == 7: the value is 5.5 at frame 10", notes));
    ScriptRunner u = started("set player/jmp 1\n", sent);
    CHECK(has_note(u, "FAIL at t.nnhs:1: player/jmp is not declared by the game (frame 10): unknown name \"player/jmp\" "
                      "(did you mean \"player/jump\"?)",
                   notes));
    ScriptRunner v = started("set player/jump 11\n", sent);
    CHECK(has_note(v, "FAIL at t.nnhs:1: player/jump: 11 is outside 0..10", notes));
}

NNH_TEST(script_runner_live_within_waits_for_a_passing_frame) {
    std::vector<wire::Message> sent;
    ScriptRunner r = started("expect-value player/jump >= 2 within 5\nend\n", sent);
    r.on_record(value_record("player/jump", 10, 1.0f));
    CHECK(r.verdict() == Verdict::Running);
    r.on_record(value_record("player/jump", 12, 2.0f));
    r.on_record(tick(14));
    CHECK(r.verdict() == Verdict::Pass);
    ScriptRunner late = started("expect-value player/jump >= 2 within 5\n", sent);
    late.on_record(value_record("player/jump", 15, 1.0f));
    CHECK(late.verdict() == Verdict::Fail);
}

NNH_TEST(script_runner_live_refusals_fail_with_their_line) {
    std::vector<wire::Message> sent;
    ScriptRunner r = started("set player/jump 6\nwait 30\nend\n", sent);
    std::uint8_t buf[16];
    r.on_record(payload_record(wire::RecordType::LiveRefused, buf,
                               wire::encode_live_refused(sent[2].seq, lv::Refusal::OutOfRange, sent[2].press, buf)));
    std::vector<std::string> notes;
    CHECK(has_note(r, "FAIL at t.nnhs:1: the game refused it (out of range): player/jump", notes));
}

NNH_TEST(script_runner_live_an_older_game_is_named) {
    ParsedScript p = parse("set a 1\n");
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.start();
    r.on_record(hello_record(1));
    const std::vector<wire::Message> sent = decoded(r);
    Record rejected;
    rejected.type = wire::RecordType::Rejected;
    rejected.seq = sent[1].seq;
    rejected.reason = wire::Reject::Type;
    r.on_record(rejected);
    std::vector<std::string> notes;
    CHECK(has_note(r, "the game has no live values (built before spec 030)", notes));
    CHECK(r.verdict() == Verdict::Error);
}

NNH_TEST(script_runner_live_tunes_steps_skip_unknown_names) {
    ParsedScript p = parse("end\n");
    Step a;
    a.command = Command::Set;
    a.text = "player/dash";
    a.value = "1";
    a.origin = "keep.toml:6";
    p.script.steps.insert(p.script.steps.begin(), a);
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.start();
    r.on_record(hello_record(1));
    (void)decoded(r);
    r.on_record(entry_record("player/jump", lv::Kind::Float, 0, lv::float_bits(10.0f)));
    r.on_record(state_record(1, true, wire::kStateCatalogueComplete));
    std::vector<std::string> notes;
    CHECK(has_note(r, "keep.toml:6: unknown name \"player/dash\": skipped", notes));
    CHECK(r.verdict() != Verdict::Fail);
}

NNH_TEST(script_runner_live_captures_dumps) {
    std::vector<wire::Message> sent;
    ScriptRunner r = started("dump\nexpect \"dump: end\" 30\n", sent);
    r.on_line("[f11 0.1s] I live   dump: begin frame 11 (1 changed of 1)");
    r.on_line("[f11 0.1s] I live   dump: [player]");
    r.on_line("[f11 0.1s] I live   dump: jump = 6 # default 5.5");
    r.on_line("[f11 0.1s] I live   dump: end");
    const std::vector<std::string> dumps = r.take_dumps();
    CHECK(dumps.size() == 1);
    CHECK(!dumps.empty() && dumps[0] == "# begin frame 11 (1 changed of 1)\n[player]\njump = 6 # default 5.5\n");
}

NNH_TEST(script_runner_live_interactive_tunes_get_and_expect_value) {
    ScriptRunner r(Script{}, RunnerOptions{.interactive = true});
    r.start();
    r.on_typed("tunes");
    r.on_record(hello_record(1));
    (void)decoded(r);
    r.on_typed("set player/jump 6"); // asks for the catalogue
    r.on_record(entry_record("player/jump", lv::Kind::Float, 0, lv::float_bits(10.0f)));
    r.on_record(state_record(3, false, wire::kStateCatalogueComplete));
    std::vector<wire::Message> sent = decoded(r);
    bool set = false;
    for (const wire::Message& m : sent) set = set || m.type == wire::MessageType::LiveSet;
    CHECK(set);
    r.on_typed("get player/jump");
    r.on_record(value_record("player/jump", 9, 6.0f));
    std::vector<std::string> notes;
    CHECK(has_note(r, "[9] player/jump = 6", notes));
    r.on_typed("expect-value player/jump == 6");
    r.on_record(value_record("player/jump", 12, 6.0f));
    CHECK(has_note(r, "expect-value player/jump == 6 passed at frame 12", notes));
}

// Review: `watch NAME every N` takes 1..255 only, never throwing.
NNH_TEST(script_runner_live_typed_watch_every_is_checked) {
    ScriptRunner r(Script{}, RunnerOptions{.interactive = true});
    r.start();
    r.on_typed("set player/jump 1");
    r.on_record(hello_record(1));
    r.on_record(entry_record("player/jump", lv::Kind::Float, 0, lv::float_bits(10.0f)));
    r.on_record(state_record(3, false, wire::kStateCatalogueComplete));
    (void)decoded(r);
    std::vector<std::string> notes;
    const char* const bad[] = {"watch player/jump every abc", "watch player/jump every 0", "watch player/jump every 256",
                               "watch player/jump every 300", "watch player/jump every 99999999999", "watch player/jump x"};
    for (const char* b : bad) {
        notes.clear();
        r.on_typed(b);
        CHECK(has_note(r, "usage: watch NAME [every N] (N 1..255)", notes));
    }
    (void)decoded(r);
    r.on_typed("watch player/jump every 255");
    const std::vector<wire::Message> sent = decoded(r);
    CHECK(sent.size() == 1 && wire::live_op(sent[0]) == wire::LiveOp::Follow && wire::live_arg(sent[0]) == 255);
}

// Review: interactive --tunes steps keep their origin (an undeclared name is noted and skipped).
NNH_TEST(script_runner_live_interactive_tunes_steps_keep_their_origin) {
    ScriptRunner r(Script{}, RunnerOptions{.interactive = true});
    r.start();
    Step s;
    s.command = Command::Set;
    s.text = "player/dash";
    s.value = "a\"b";
    s.origin = "keep.toml:6";
    Step resume;
    resume.command = Command::Resume;
    r.on_typed_steps({s, resume});
    r.on_record(hello_record(1));
    r.on_record(entry_record("player/jump", lv::Kind::Float, 0, lv::float_bits(10.0f)));
    r.on_record(state_record(1, true, wire::kStateCatalogueComplete));
    std::vector<std::string> notes;
    CHECK(has_note(r, "keep.toml:6: unknown name \"player/dash\": skipped", notes));
}
