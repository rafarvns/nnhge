// The session and the apply point (spec 031 T2.5, FR-6, FR-18, Q10): a batch stamped F applies at F's point of
// nnh::frame() (before the frame's events reach the game); frame 0 = the next frame; a late Apply answers Late and
// applies at once; while paused the files still arrive and the apply waits for a step or a resume; a disconnect mid-
// transfer removes the files; no reload record before the PC spoke reload.

#include <cstring>

#include "nnh/assets.hpp"
#include "nnh/events.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform/system.hpp"
#include "reload_fixture.hpp"
#include "remote/live_wire.hpp"

using namespace reload_test;

namespace {

const char kHero[] = "old hero";

void start_game(char* index, std::size_t capacity) {
    const Put assets[] = {text("art/hero.png", kHero)};
    start(assets, 1, index, capacity);
}

struct Seen {
    int events = 0;
    std::uint64_t frame = 0;
    bool reloaded = false;
    std::uint32_t count = 0;
};

} // namespace

NNH_TEST(reload_apply_a_batch_lands_on_its_stamped_frame) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    Seen seen;
    auto sub = nnh::events::subscribe<nnh::AssetsReloaded>([&seen](const nnh::AssetsReloaded& e) {
        ++seen.events;
        seen.frame = nnh::frame_count();
        seen.reloaded = nnh::assets::reloaded("art/hero");
        seen.count = e.count;
    });
    const Put batch[] = {text("art/hero.png", "new")};
    send_batch(1, batch, 1, 6);
    char out[32];
    for (int f = 2; f <= 5; ++f) {
        CHECK(nnh::frame());
        CHECK(read_text("art/hero.png", out, sizeof out) == 8); // old until frame 6
    }
    CHECK(records().staged == 1);
    CHECK(nnh::frame()); // frame 6
    CHECK(nnh::frame_count() == 6);
    CHECK(read_text("art/hero.png", out, sizeof out) == 3);
    CHECK(seen.events == 1 && seen.frame == 6 && seen.reloaded && seen.count == 1);
    const Records r = records();
    CHECK(r.applied == 1);
    CHECK(r.last_applied.frame == 6 && r.last_applied.batch == 1 && r.last_applied.count == 1);
    nnh::shutdown();
}

NNH_TEST(reload_apply_frame_zero_is_the_next_frame) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    const Put batch[] = {text("art/hero.png", "new")};
    send_batch(1, batch, 1, 0);
    CHECK(nnh::frame()); // frame 2 reads it all and applies
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(records().last_applied.frame == 2);
    nnh::shutdown();
}

NNH_TEST(reload_apply_a_late_apply_answers_late_and_applies_now) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    frames(5); // now frame 6
    const Put batch[] = {text("art/hero.png", "new")};
    send_batch(1, batch, 1, 3);
    CHECK(nnh::frame()); // frame 7
    CHECK(nnh::assets::reload_count() == 1);
    const remote_test::Sent s = remote_test::sent();
    CHECK(s.count_of(wire::RecordType::Late) == 1);
    CHECK(records().last_applied.frame == 7);
    nnh::shutdown();
}

NNH_TEST(reload_apply_waits_while_paused_and_lands_on_the_step) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    // Pause at frame 2; the batch (40 000 bytes: three reads' worth) and its Apply arrive with it. Frame 2 holds: its
    // paused ticks keep receiving the files; the Apply is due, but nothing applies until the game frame runs.
    wire::Message pause = wire::pause_message(wire::PauseOp::Pause, 0);
    pause.frame = 2;
    send(pause);
    static char big[40000];
    std::memset(big, 'n', sizeof big);
    const Put batch[] = {Put{"art/hero.png", big, sizeof big}};
    send_batch(1, batch, 1, 2);
    fake::queue_dev_requests({0, 0, 0, 0, nnh::platform::kDevStepFrame}); // four paused ticks, then F10
    CHECK(nnh::frame());
    CHECK(nnh::frame_count() == 2); // the stepped frame
    CHECK(records().staged == 1);   // the files arrived on the paused ticks
    CHECK(nnh::assets::reload_count() == 1);
    CHECK(records().last_applied.frame == 2);
    wire::Message resume = wire::pause_message(wire::PauseOp::Resume, 0);
    send(resume);
    CHECK(nnh::frame());
    nnh::shutdown();
}

NNH_TEST(reload_apply_a_disconnect_mid_transfer_removes_the_files) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    static char big[40000];
    std::memset(big, 'b', sizeof big);
    const Put batch[] = {text("a.txt", "a"), Put{"art/hero.png", big, sizeof big}};
    send_batch(1, batch, 2, -1);
    CHECK(nnh::frame()); // at most 16 KB of payload per frame: the batch is still open
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == 1);
    CHECK(records().staged == 0);
    fake::remote_disconnect();
    frames(4);
    CHECK(nnh::platform::fs_size(".nnh_reload/1.bin") == -1);
    CHECK(nnh::platform::fs_size(".nnh_reload/2.bin") == -1);
    CHECK(nnh::assets::reload_count() == 0);
    nnh::shutdown();
}

NNH_TEST(reload_apply_reads_at_most_16_kb_of_payload_a_frame) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    static char big[50000];
    std::memset(big, 'z', sizeof big);
    const Put batch[] = {Put{"art/hero.png", big, sizeof big}};
    send_batch(1, batch, 1, 0);
    CHECK(nnh::frame());
    // The first read takes up to 1 KB of messages (and the payload they bring along), then 16 KB of payload at most.
    const std::int64_t first = nnh::platform::fs_size(".nnh_reload/1.bin");
    CHECK(first > 0 && first <= 16384 + 1024);
    int frames_taken = 1;
    while (nnh::assets::reload_count() == 0 && frames_taken < 20) {
        CHECK(nnh::frame());
        ++frames_taken;
    }
    CHECK(frames_taken >= 3 && frames_taken <= 4); // 50 000 bytes at about 17 KB a frame
    CHECK(nnh::assets::size("art/hero.png").value_or(0) == sizeof big);
    nnh::shutdown();
}

NNH_TEST(reload_apply_no_record_before_the_pc_speaks_reload) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    frames(5);
    const Records r = records();
    CHECK(r.staged == 0 && r.applied == 0 && r.refused == 0);
    nnh::shutdown();
}

NNH_TEST(reload_apply_a_transfer_refusal_is_one_warning_and_one_record) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    Lines warn("reload refused");
    send(wire::reload_begin_message(1, 1, 100));
    send(wire::reload_chunk_message(0, 4, 0)); // no file open
    fake::remote_push("abcd", 4);
    send(wire::reload_chunk_message(0, 4, 4)); // the rest of the dropped batch: quiet
    fake::remote_push("efgh", 4);
    frames(2);
    CHECK(warn.count == 1);
    CHECK(std::strstr(warn.last, "transfer: out of order") != nullptr);
    const Records r = records();
    CHECK(r.refused == 1);
    CHECK(r.last_refused.stage == wire::ReloadStage::Transfer && r.last_refused.reason == wire::ReloadReason::Order);
    // a refusal is not a malformed message: the remote stays on
    send(wire::reload_begin_message(2, 1, 5));
    send(wire::reload_file_message(0, wire::ReloadOp::Remove, 5, 0));
    fake::remote_push("x.png", 5);
    frames(1);
    CHECK(records().staged == 1);
    nnh::shutdown();
}

NNH_TEST(reload_apply_a_notice_is_logged_with_its_path) {
    RELOAD_DEV_ONLY();
    char index[256];
    start_game(index, sizeof index);
    Lines warn("reload refused");
    send(wire::reload_notice_message(wire::NoticeReason::ConversionFailed, 22, 0));
    fake::remote_push("assets/levels/room.tmj", 22);
    frames(1);
    CHECK(warn.count == 1);
    CHECK(std::strstr(warn.last, "assets/levels/room.tmj: conversion failed (see nnh)") != nullptr);
    CHECK(records().last_refused.stage == wire::ReloadStage::Notice);
    send(wire::reload_notice_message(wire::NoticeReason::NeedsRestart, 14, 0));
    fake::remote_push("data_types.cpp", 14);
    frames(1);
    CHECK(std::strstr(warn.last, "data_types.cpp: needs a rebuild and restart") != nullptr);
    CHECK(records().last_refused.stage == wire::ReloadStage::Restart);
    nnh::shutdown();
}
