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
                           [&f](bool) {
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
    CHECK(c.take_bytes().size() == kInitialAllowance);
    CHECK(c.take_bytes().empty()); // no tick yet
    Record tick;
    tick.type = wire::RecordType::Tick;
    c.on_record(tick);
    CHECK(c.take_bytes().size() == kAllowancePerTick);
    for (int i = 0; i < 4; ++i) c.on_record(tick);
    c.take_bytes();
    CHECK(c.state() == ReloadController::State::Waiting);
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
