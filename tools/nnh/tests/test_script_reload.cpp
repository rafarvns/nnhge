// Reload lines in scripts (spec 031 T3.6, FR-22, Q11): syntax and line errors, paths outside the game's folder, the
// paused-section rules; backups restored byte for byte, the journal, an interrupted run; the runner against a fake game
// and builder: a held swap pauses at F, sends the batch with its Apply and a Resume for F, and goes on at F + 1;
// expect-reload passing and failing; an unheld swap waited for with within N.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "reload/asset_swap.hpp"
#include "reload/reload_controller.hpp"
#include "reload/reload_wire.hpp"
#include "remote/live_wire.hpp"
#include "remote/script.hpp"
#include "remote/script_runner.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
using nnh::tool::reload::AssetSwaps;
using nnh::tool::reload::BuildOutcome;
using nnh::tool::reload::ControllerSetup;
using nnh::tool::reload::ReloadController;
namespace fs = std::filesystem;

namespace {

ParsedScript parse(const char* text) { return parse_script(text, "t.nnhs"); }

std::string dir(const std::string& name) {
    const std::string d = std::string(NNH_TOOL_TEST_TMP) + "/scriptreload_" + name;
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

std::string read(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return "<missing>";
    std::string s;
    char b[256];
    std::size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    std::fclose(f);
    return s;
}

Record payload_record(wire::RecordType type, const std::uint8_t* bytes, std::size_t n) {
    Record r;
    r.type = type;
    r.payload.assign(bytes + wire::kRecordHeaderSize, bytes + n);
    return r;
}

Record hello_record() {
    Record r;
    r.type = wire::RecordType::Hello;
    r.version = wire::kVersion;
    r.frame = 1;
    return r;
}

Record state_record(std::uint32_t frame, bool paused) {
    std::uint8_t buf[16];
    return payload_record(wire::RecordType::LiveState, buf, wire::encode_live_state(frame, paused, 0, 0, buf));
}

Record applied_record(std::uint16_t batch, std::uint32_t frame) {
    std::uint8_t buf[wire::kMaxReloadRecord];
    return payload_record(wire::RecordType::ReloadApplied, buf,
                          wire::encode_reload_applied({batch, 1, frame, 1, 50}, buf));
}

Record tick(std::uint32_t frame) {
    Record r;
    r.type = wire::RecordType::Tick;
    r.frame = frame;
    return r;
}

std::vector<wire::Message> messages_in(const std::vector<std::uint8_t>& bytes) {
    std::vector<wire::Message> out;
    for (std::size_t at = 0; at + wire::kMessageSize <= bytes.size();) {
        wire::Message m;
        if (wire::decode_message(bytes.data() + at, m) != wire::Reject::None) break;
        out.push_back(m);
        at += wire::kMessageSize + wire::payload_length(m);
    }
    return out;
}

/// A game folder with assets/a.txt and its converted out/a.txt; the fake build converts by copying.
struct Game {
    std::string root, game, out, script;
    Game(const char* name) {
        root = dir(name);
        game = root + "/game";
        out = root + "/out";
        script = root + "/scripts";
        write(game + "/assets/a.txt", "original");
        write(out + "/a.txt", "original");
        write(script + "/fixtures/a_new.txt", "fixture bytes");
    }
    ControllerSetup setup() {
        const std::string g = game, o = out;
        return ControllerSetup{o, g + "/assets",
                               [g, o](bool) {
                                   write(o + "/a.txt", read(g + "/assets/a.txt"));
                                   BuildOutcome b;
                                   b.ok = true;
                                   return b;
                               },
                               [] { return 0LL; }};
    }
};

} // namespace

NNH_TEST(script_reload_lines_parse) {
    const ParsedScript p = parse("swap-asset assets/art/hero.png fixtures/hero_big.png\n"
                                 "swap-asset data_types.cpp fixtures/changed.cpp unheld\n"
                                 "expect-reload art/hero within 600\n"
                                 "expect-reload refused\n"
                                 "expect-reload\n"
                                 "restore-asset assets/art/hero.png\n");
    CHECK(p.errors.empty());
    CHECK(p.script.steps.size() == 7);
    CHECK(p.script.steps[0].command == Command::SwapAsset && p.script.steps[0].text == "assets/art/hero.png" &&
          p.script.steps[0].value == "fixtures/hero_big.png" && !p.script.steps[0].unheld);
    CHECK(p.script.steps[1].unheld);
    CHECK(p.script.steps[2].command == Command::ExpectReload && p.script.steps[2].text == "art/hero" &&
          p.script.steps[2].frames == 600);
    CHECK(p.script.steps[3].refused && p.script.steps[3].text.empty());
    CHECK(p.script.steps[5].command == Command::RestoreAsset);
}

NNH_TEST(script_reload_errors_name_their_lines) {
    const ParsedScript p = parse("swap-asset ../outside.png fixtures/a.png\n"
                                 "swap-asset assets/a.png /abs/fixture.png\n"
                                 "swap-asset assets/a.png f.png later\n"
                                 "expect-reload a b\n"
                                 "restore-asset\n"
                                 "pause\n"
                                 "swap-asset assets/a.png f.png\n"
                                 "expect-reload within 10\n");
    CHECK(p.errors.size() == 7);
    CHECK(p.errors[0].find("t.nnhs:1: SOURCE must be a relative path") == 0);
    CHECK(p.errors[1].find("t.nnhs:2: FIXTURE") == 0);
    CHECK(p.errors[2].find("t.nnhs:3: usage: swap-asset") == 0);
    CHECK(p.errors[3].find("t.nnhs:4: usage: expect-reload") == 0);
    CHECK(p.errors[4].find("t.nnhs:5: wrong number of arguments") == 0);
    CHECK(p.errors[5].find("t.nnhs:7: swap-asset while paused") == 0);
    CHECK(p.errors[6].find("t.nnhs:8: expect-reload while paused") == 0);
}

NNH_TEST(script_reload_swaps_are_restored_byte_for_byte) {
    Game g("swaps");
    AssetSwaps swaps(g.game, g.root + "/backup");
    CHECK(swaps.swap("assets/a.txt", g.script + "/fixtures/a_new.txt").empty());
    CHECK(read(g.game + "/assets/a.txt") == "fixture bytes");
    CHECK(swaps.journal_left());
    CHECK(swaps.swap("assets/a.txt", g.script + "/fixtures/a_new.txt").empty()); // twice: the first backup stays
    CHECK(swaps.restore_all() == 1);
    CHECK(read(g.game + "/assets/a.txt") == "original");
    CHECK(!swaps.journal_left());
    CHECK(!swaps.swap("../x", "y").empty());
    CHECK(!swaps.swap("assets/missing.txt", g.script + "/fixtures/a_new.txt").empty());
}

NNH_TEST(script_reload_an_interrupted_run_leaves_a_journal_the_next_restores) {
    Game g("journal");
    {
        AssetSwaps first(g.game, g.root + "/backup");
        CHECK(first.swap("assets/a.txt", g.script + "/fixtures/a_new.txt").empty());
    } // the process "dies" here: nothing restored
    CHECK(nnh::tool::reload::journal_game_dir(g.root + "/backup") == g.game); // nnh restore-assets finds the game
    AssetSwaps next(g.game, g.root + "/backup");
    CHECK(next.journal_left());
    CHECK(next.restore_all() == 1);
    CHECK(read(g.game + "/assets/a.txt") == "original");
}

NNH_TEST(script_reload_a_held_swap_lands_on_its_frame) {
    Game g("held");
    ReloadController controller(g.setup());
    controller.start();
    AssetSwaps swaps(g.game, g.root + "/backup");
    ParsedScript p = parse("wait 5\nswap-asset assets/a.txt fixtures/a_new.txt\nexpect-reload a\nwait 3\nend\n");
    CHECK(p.errors.empty());
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.set_reload(&controller);
    r.set_reload_script(&swaps, g.script);
    r.start();
    r.take_messages();
    r.on_record(hello_record());
    // the pause for F = 1 + 5 = 6
    bool paused_at_6 = false;
    for (const MessageBytes& b : r.take_messages()) {
        wire::Message m;
        CHECK(wire::decode_message(b.data(), m) == wire::Reject::None);
        paused_at_6 = paused_at_6 || (m.type == wire::MessageType::Pause && m.frame == 6);
    }
    CHECK(paused_at_6);
    CHECK(read(g.game + "/assets/a.txt") == "fixture bytes");
    r.on_record(state_record(6, true)); // paused: build and send
    const std::vector<wire::Message> sent = messages_in(r.take_bytes());
    CHECK(!sent.empty() && sent.front().type == wire::MessageType::ReloadBegin);
    CHECK(sent.size() >= 2 && sent[sent.size() - 2].type == wire::MessageType::ReloadApply &&
          sent[sent.size() - 2].frame == 6);
    CHECK(sent.back().type == wire::MessageType::Pause && sent.back().frame == 6); // the resume
    r.on_record(applied_record(1, 6));
    r.on_record(tick(12));
    CHECK(r.finished() && r.verdict() == Verdict::Pass);
    swaps.restore_all();
    CHECK(read(g.game + "/assets/a.txt") == "original");
}

NNH_TEST(script_reload_expect_reload_fails_on_a_refusal) {
    Game g("refused");
    ReloadController controller(g.setup());
    controller.start();
    AssetSwaps swaps(g.game, g.root + "/backup");
    ParsedScript p = parse("swap-asset assets/a.txt fixtures/a_new.txt\nexpect-reload\nend\n");
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.set_reload(&controller);
    r.set_reload_script(&swaps, g.script);
    r.start();
    r.on_record(hello_record());
    r.take_messages();
    r.on_record(state_record(1, true));
    r.take_bytes();
    std::uint8_t buf[wire::kMaxReloadRecord];
    r.on_record(payload_record(
        wire::RecordType::ReloadRefused, buf,
        wire::encode_reload_refused({1, wire::ReloadStage::Validate, wire::ReloadReason::Corrupted, 0, 1, 0}, buf)));
    CHECK(r.finished() && r.verdict() == Verdict::Fail);
    bool said = false;
    for (const std::string& n : r.take_notes()) said = said || n.find("t.nnhs:2: expect-reload: refused") != std::string::npos;
    CHECK(said);
    swaps.restore_all();
}

NNH_TEST(script_reload_an_unheld_swap_is_waited_for_within_n) {
    Game g("unheld");
    ReloadController controller(g.setup());
    controller.start();
    AssetSwaps swaps(g.game, g.root + "/backup");
    ParsedScript p = parse("swap-asset assets/a.txt fixtures/a_new.txt unheld\nexpect-reload a within 60\nend\n");
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.set_reload(&controller);
    r.set_reload_script(&swaps, g.script);
    r.start();
    r.on_record(hello_record());
    const std::vector<wire::Message> sent = messages_in(r.take_bytes());
    CHECK(!sent.empty() && sent.back().type == wire::MessageType::ReloadApply && sent.back().frame == 0);
    r.on_record(tick(20));
    CHECK(!r.finished());
    r.on_record(applied_record(1, 24));
    r.on_record(tick(30));
    CHECK(r.finished() && r.verdict() == Verdict::Pass);
    swaps.restore_all();
}

NNH_TEST(script_reload_without_hot_reload_is_an_error) {
    ParsedScript p = parse("swap-asset assets/a.txt fixtures/a_new.txt\nend\n");
    ScriptRunner r(std::move(p.script), RunnerOptions{});
    r.start();
    r.on_record(hello_record());
    CHECK(r.finished() && r.verdict() == Verdict::Error);
}
