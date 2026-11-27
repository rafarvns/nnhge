// The reload cycle on the PC (spec 031 T3.5, FR-1, FR-3, FR-14, FR-15): a fake builder and game. A save after the quiet
// period builds and sends one batch; a failed build sends a notice and never a batch; a dry run with commands refuses
// ("rebuild and restart"); a refused batch keeps the mirror (the next cycle sends it again); a game that rejects the
// message type turns watching off; pacing by ticks; the printed lines.

#include <cstdio>
#include <filesystem>
#include <string>

#include "reload/reload_controller.hpp"
#include "reload/reload_wire.hpp"
#include "test.hpp"

using namespace nnh::tool::reload;
namespace fs = std::filesystem;
namespace wire = nnh::detail::remote;
using nnh::tool::remote::Record;

namespace {

struct Fake {
    std::string src, out;
    long long now = 1000;
    int builds = 0;
    bool fail = false;
    std::string restart;
    // What the build writes into out/a.txt: another size than "v1" (a write in the same clock tick keeps the time)
    std::string next_out_text = "v2 changed";
};

std::string dir(const char* name) {
    const std::string d = std::string(NNH_TOOL_TEST_TMP) + "/reloadctl_" + name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

void write(const std::string& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

ControllerSetup setup(Fake& f) {
    return ControllerSetup{f.out, f.src,
                           [&f](const std::string&) {
                               ++f.builds;
                               BuildOutcome b;
                               b.ok = !f.fail;
                               b.restart = f.restart;
                               if (f.fail) b.lines.push_back("assets/a.txt:1: broken");
                               else write(f.out + "/a.txt", f.next_out_text);
                               b.ms = 210;
                               return b;
                           },
                           [&f] { return f.now; }, std::string()};
}

Record applied(std::uint16_t batch, std::uint32_t frame) {
    std::uint8_t out[wire::kMaxReloadRecord];
    wire::encode_reload_applied({batch, 1, frame, 1, 100}, out);
    Record r;
    r.type = wire::RecordType::ReloadApplied;
    r.payload.assign(out + wire::kRecordHeaderSize, out + wire::kRecordHeaderSize + wire::kReloadAppliedPayload);
    return r;
}

Record refused(std::uint16_t batch) {
    std::uint8_t out[wire::kMaxReloadRecord];
    wire::encode_reload_refused({batch, wire::ReloadStage::Validate, wire::ReloadReason::Corrupted, 0, 9, 0}, out);
    Record r;
    r.type = wire::RecordType::ReloadRefused;
    r.payload.assign(out + wire::kRecordHeaderSize, out + wire::kRecordHeaderSize + wire::kReloadRefusedPayload);
    return r;
}

bool has(const std::vector<std::string>& notes, const std::string& text) {
    for (const std::string& n : notes) {
        if (n.find(text) != std::string::npos) return true;
    }
    return false;
}

Fake make(const char* name) {
    Fake f;
    f.src = dir((std::string(name) + "_src").c_str());
    f.out = dir((std::string(name) + "_out").c_str());
    write(f.src + "/a.txt", "v1");
    write(f.out + "/a.txt", "v1");
    return f;
}

} // namespace

NNH_TEST(reload_controller_a_save_builds_and_sends_one_batch) {
    Fake f = make("save");
    ReloadController c(setup(f));
    c.start();
    c.set_watching(true);
    write(f.src + "/a.txt", "v2 longer"); // saved
    for (int i = 0; i < 3; ++i) {
        f.now += 100;
        c.poll();
    }
    CHECK(f.builds == 0); // the quiet period
    f.now += 300;
    c.poll();
    CHECK(f.builds == 1);
    CHECK(c.state() == ReloadController::State::Sending);
    const std::vector<std::uint8_t> bytes = c.take_bytes();
    CHECK(bytes.size() >= wire::kMessageSize);
    if (bytes.size() < wire::kMessageSize) return;
    wire::Message m;
    CHECK(wire::decode_message(bytes.data(), m) == wire::Reject::None && m.type == wire::MessageType::ReloadBegin);
    CHECK(c.state() == ReloadController::State::Waiting);
    f.now += 400;
    c.on_record(applied(1, 77));
    CHECK(c.state() == ReloadController::State::Idle);
    CHECK(c.outcome().applied && c.outcome().frame == 77);
    const std::vector<std::string> notes = c.take_notes();
    CHECK(has(notes, "reload 1: 1 files, 0.0 KB (build 0.21 s)"));
    CHECK(has(notes, "reload 1 applied at frame 77,"));
    CHECK(has(notes, "after the save: a"));
}

NNH_TEST(reload_controller_a_failed_build_sends_a_notice_never_a_batch) {
    Fake f = make("fail");
    f.fail = true;
    ReloadController c(setup(f));
    c.start();
    CHECK(!c.run_cycle(0));
    const std::vector<std::uint8_t> bytes = c.take_bytes();
    CHECK(bytes.size() >= wire::kMessageSize);
    if (bytes.size() < wire::kMessageSize) return;
    wire::Message m;
    CHECK(wire::decode_message(bytes.data(), m) == wire::Reject::None && m.type == wire::MessageType::ReloadNotice);
    CHECK(wire::notice_reason(m) == wire::NoticeReason::ConversionFailed);
    const std::vector<std::string> notes = c.take_notes();
    CHECK(has(notes, "assets/a.txt:1: broken"));
    CHECK(has(notes, "conversion failed"));
    CHECK(c.state() == ReloadController::State::Idle);
}

NNH_TEST(reload_controller_a_script_source_names_the_notice_and_is_checked_when_outside) {
    Fake f = make("scriptsource");
    f.fail = true;
    std::string checked = "unset";
    ControllerSetup s = setup(f);
    const auto inner = s.build;
    s.build = [&checked, inner](const std::string& outside) {
        checked = outside;
        return inner(outside);
    };
    s.game_dir = fs::path(f.src).parent_path().string() + "/game";
    s.src_dir = s.game_dir + "/assets";
    ReloadController c(s);
    c.start();
    CHECK(!c.run_cycle(0, {}, "assets/levels/room.tmj"));
    CHECK(checked.empty()); // inside the assets folder: no program check
    const std::vector<std::uint8_t> bytes = c.take_bytes();
    CHECK(bytes.size() >= wire::kMessageSize + 15);
    if (bytes.size() < wire::kMessageSize + 15) return;
    wire::Message m;
    CHECK(wire::decode_message(bytes.data(), m) == wire::Reject::None && wire::reload_path_length(m) == 15);
    CHECK(std::string(bytes.begin() + wire::kMessageSize, bytes.begin() + wire::kMessageSize + 15) == "levels/room.tmj");
    CHECK(!c.run_cycle(0, {}, "data_types.cpp"));
    CHECK(checked == s.game_dir + "/data_types.cpp");
}

NNH_TEST(reload_controller_a_dry_run_with_commands_needs_a_restart) {
    Fake f = make("restart");
    f.restart = "Building CXX object examples/data/CMakeFiles/data.dir/data_types.cpp.obj";
    ReloadController c(setup(f));
    c.start();
    CHECK(!c.run_cycle(0));
    const std::vector<std::uint8_t> bytes = c.take_bytes();
    CHECK(bytes.size() >= wire::kMessageSize);
    if (bytes.size() < wire::kMessageSize) return;
    wire::Message m;
    CHECK(wire::decode_message(bytes.data(), m) == wire::Reject::None && wire::notice_reason(m) == wire::NoticeReason::NeedsRestart);
    CHECK(has(c.take_notes(), "data_types.cpp.obj: rebuild and restart"));
}

NNH_TEST(reload_controller_a_refused_batch_keeps_the_mirror) {
    Fake f = make("refused");
    ReloadController c(setup(f));
    c.start();
    CHECK(c.run_cycle(0));
    c.take_bytes();
    c.on_record(refused(1));
    CHECK(c.state() == ReloadController::State::Idle && !c.outcome().applied);
    CHECK(has(c.take_notes(), "refused by the game: a.txt: corrupted"));
    CHECK(c.run_cycle(0)); // the same change again: the mirror didn't move
    CHECK(c.outcome().paths.size() == 1);
}

NNH_TEST(reload_controller_an_applied_batch_moves_the_mirror) {
    Fake f = make("moved");
    ReloadController c(setup(f));
    c.start();
    CHECK(c.run_cycle(0));
    c.take_bytes();
    c.on_record(applied(1, 50));
    CHECK(c.outcome().applied);
    CHECK(!c.run_cycle(0)); // the same output again: already on the console
    CHECK(has(c.take_notes(), "nothing changed"));
}

NNH_TEST(reload_controller_a_batch_refused_for_a_restart_ends_the_cycle) {
    Fake f = make("restartrefused");
    ReloadController c(setup(f));
    c.start();
    CHECK(c.run_cycle(0));
    c.take_bytes();
    std::uint8_t out[wire::kMaxReloadRecord];
    wire::encode_reload_refused({1, wire::ReloadStage::Restart, wire::ReloadReason::Schema, 0, 9, 0}, out);
    Record r;
    r.type = wire::RecordType::ReloadRefused;
    r.payload.assign(out + wire::kRecordHeaderSize, out + wire::kRecordHeaderSize + wire::kReloadRefusedPayload);
    c.on_record(r); // the game's verdict on batch 1, not the echo of a notice (batch 0)
    CHECK(c.state() == ReloadController::State::Idle && c.outcome().done && !c.outcome().applied);
}

NNH_TEST(reload_controller_nothing_changed_sends_nothing) {
    Fake f = make("nothing");
    f.next_out_text = "v1";
    ReloadController c(setup(f));
    c.start();
    CHECK(!c.run_cycle(0));
    CHECK(c.take_bytes().empty());
    CHECK(has(c.take_notes(), "nothing changed"));
}

NNH_TEST(reload_controller_an_older_game_turns_watching_off) {
    Fake f = make("older");
    ReloadController c(setup(f));
    c.start();
    c.set_watching(true);
    CHECK(c.run_cycle(0));
    c.take_bytes();
    Record r;
    r.type = wire::RecordType::Rejected;
    r.reason = wire::Reject::Type;
    c.on_record(r);
    CHECK(!c.enabled() && !c.watching());
    CHECK(has(c.take_notes(), "built before spec 031): watching off"));
}

NNH_TEST(reload_controller_pacing_follows_the_ticks) {
    Fake f = make("pace");
    f.next_out_text = std::string(200 * 1024, 'p');
    ReloadController c(setup(f));
    c.start();
    CHECK(c.run_cycle(0));
    const std::size_t first = c.take_bytes().size();
    CHECK(first > 0 && first <= kInitialAllowance);
    CHECK(c.take_bytes().empty()); // no tick yet
    Record tick;
    tick.type = wire::RecordType::Tick;
    c.on_record(tick);
    const std::size_t second = c.take_bytes().size();
    CHECK(second > 0 && first + second <= kInitialAllowance + kAllowancePerTick);
    for (int i = 0; i < 4; ++i) c.on_record(tick);
    c.take_bytes();
    CHECK(c.state() == ReloadController::State::Waiting);
}

namespace {

/// The messages of `bytes` if it is whole units (each message followed by its whole payload), else -1.
int whole_units(const std::vector<std::uint8_t>& bytes) {
    int count = 0;
    std::size_t at = 0;
    while (at < bytes.size()) {
        wire::Message m;
        if (bytes.size() - at < wire::kMessageSize || wire::decode_message(bytes.data() + at, m) != wire::Reject::None) {
            return -1;
        }
        at += wire::kMessageSize + wire::payload_length(m);
        ++count;
    }
    return at == bytes.size() ? count : -1;
}

} // namespace

NNH_TEST(reload_controller_slices_are_whole_units) {
    Fake f = make("units");
    f.next_out_text = std::string(200 * 1024 + 7, 'u');
    ReloadController c(setup(f));
    c.start();
    CHECK(c.run_cycle(0));
    Record tick;
    tick.type = wire::RecordType::Tick;
    int slices = 0;
    std::size_t sent = 0;
    for (int i = 0; i < 10 && c.sending(); ++i) {
        const std::vector<std::uint8_t> slice = c.take_bytes();
        sent += slice.size();
        CHECK(sent <= kInitialAllowance + static_cast<std::size_t>(i) * kAllowancePerTick); // never ahead of the game
        CHECK(whole_units(slice) > 0); // another message sent between two slices lands on a boundary
        ++slices;
        c.on_record(tick);
    }
    CHECK(slices >= 4 && !c.sending());
}

NNH_TEST(reload_controller_a_refusal_mid_send_keeps_the_resume) {
    Fake f = make("midsend");
    f.next_out_text = std::string(200 * 1024, 'm');
    ReloadController c(setup(f));
    c.start();
    wire::Message resume;
    resume.type = wire::MessageType::Pause;
    resume.frame = 9;
    std::vector<std::uint8_t> tail(wire::kMessageSize);
    wire::encode_message(resume, tail.data());
    CHECK(c.run_cycle(9, tail));
    CHECK(whole_units(c.take_bytes()) > 0); // part of the batch went out
    c.on_record(refused(1));                // the game refused it meanwhile
    CHECK(c.state() == ReloadController::State::Idle);
    const std::vector<std::uint8_t> rest = c.take_bytes();
    CHECK(rest == tail); // only the held swap's Resume is left, and it still goes out
    CHECK(!c.sending());
}

NNH_TEST(reload_controller_typed_reload_of_one_path) {
    Fake f = make("typed");
    f.next_out_text = "v1"; // the build changes nothing
    ReloadController c(setup(f));
    c.start();
    c.request("a.txt");
    c.poll();
    CHECK(f.builds == 1);
    CHECK(c.outcome().paths.size() == 1 && c.outcome().paths[0] == "a.txt");
}
