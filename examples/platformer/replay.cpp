#include "replay.hpp"

#include "replay_script.hpp"

#include <cstdio>
#include <nnh/debug.hpp>
#include <nnh/events.hpp>
#include <nnh/gfx.hpp>
#include <nnh/log.hpp>
#include <nnh/time.hpp>

NNH_LOG_CHANNEL_EXTERN(platformer);

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;
constexpr nnh::Screen kBottom = nnh::Screen::Bottom;
const nnh::Color kPass = nnh::rgb(80, 220, 100);
const nnh::Color kFail = nnh::rgb(240, 70, 60);
const nnh::Color kBusy = nnh::rgb(250, 220, 120);

bool all_ok(const ReplayRun& run) {
    for (int i = 0; i < run.segment_count(); ++i) {
        if (!run.meter(i).ok()) return false;
    }
    return run.segment_count() > 0;
}

/// "level1 worst 17.2 ms, missed 0/599" (log and screen)
void meter_text(char* out, std::size_t size, const ReplayRun& run, int segment) {
    const FrameMeter& m = run.meter(segment);
    std::snprintf(out, size, "%s worst %.1f ms, missed %d/%d", level_name(run.segment_info(segment).level),
                  m.worst_ms(), m.missed(), m.frames());
}

} // namespace

void Replay::start() {
    int count = 0;
    const ReplaySegment* segments = replay_segments(count);
    run_ = ReplayRun(segments, count);
    running_ = true;
    finished_ = false;
    NNH_LOG_INFO(platformer, "replay: started, %d frames: %s %d, then %s %d; no buttons until the verdict",
                 run_.total_frames(), level_name(segments[0].level), segments[0].frames,
                 level_name(segments[count - 1].level), segments[count - 1].frames);
    nnh::events::publish(ShowDemo{run_.level()});
}

void Replay::stop() {
    if (!running_) return;
    running_ = false;
    NNH_LOG_INFO(platformer, "replay: stopped at frame %d of %d: no verdict", run_.total_frame(),
                 run_.total_frames());
}

Replay::Drive Replay::drive(Demo level) const {
    if (!running_) return {};
    if (level != run_.level()) return {.active = true, .frozen = true};
    return {.active = true, .restart = run_.frame() == 0, .input = run_.input()};
}

void Replay::record(Demo level, const ReplaySample& sample) {
    if (!running_ || level != run_.level()) return;
    playing_ = level;
    const ReplayProgress p = run_.record(sample, nnh::time::delta().seconds() * 1000.0);
    if (p.checkpoint && !p.finished) log_checkpoint(sample); // the last frame's checksum is in the verdict
    if (p.finished) {
        finish();
    } else if (p.segment_end) {
        nnh::events::publish(ShowDemo{run_.level()});
    }
}

void Replay::log_checkpoint(const ReplaySample& s) const {
    // Whole pixels and the checksum only: text the PC and the console print the same way.
    NNH_LOG_INFO(platformer, "replay: %d %s x %d y %d coins %d sum %08X", run_.total_frame(),
                 level_name(playing_), static_cast<int>(s.box.x), static_cast<int>(s.box.y), s.coins,
                 static_cast<unsigned>(run_.checksum()));
}

void Replay::finish() {
    running_ = false;
    finished_ = true;
    // One line (a script can expect only one line per frame): the final checksum and the 60 Hz verdict.
    char first[48], second[48];
    meter_text(first, sizeof first, run_, 0);
    meter_text(second, sizeof second, run_, run_.segment_count() - 1);
    const unsigned sum = run_.checksum();
    if (all_ok(run_))
        NNH_LOG_INFO(platformer, "replay: done %d frames sum %08X: 60 Hz PASS (%s; %s)", run_.total_frame(),
                     sum, first, second);
    else
        NNH_LOG_WARN(platformer, "replay: done %d frames sum %08X: 60 Hz FAIL (%s; %s)", run_.total_frame(),
                     sum, first, second);
}

void Replay::draw_panel(float y) const {
    namespace gfx = nnh::gfx;
    const float line = static_cast<float>(nnh::debug::line_height());
    if (running_) {
        gfx::draw_text(kBottom, {12, y}, "REPLAY RUNNING", {.color = kBusy, .scale = 2, .shadow = true});
        nnh::debug::printf(kBottom, 12, static_cast<int>(y + 24), "frame %d of %d (%s)", run_.total_frame(),
                           run_.total_frames(), level_name(run_.level()));
        nnh::debug::printf(kBottom, 12, static_cast<int>(y + 24 + line), "checksum %08X",
                           static_cast<unsigned>(run_.checksum()));
        nnh::debug::print(kBottom, 12, static_cast<int>(y + 24 + 2 * line), "No buttons. SELECT stops it.");
        return;
    }
    if (!finished_) return;
    const bool pass = all_ok(run_);
    gfx::draw_text(kBottom, {12, y}, pass ? "REPLAY: 60 Hz PASS" : "REPLAY: 60 Hz FAIL",
                   {.color = pass ? kPass : kFail, .scale = 2, .shadow = true});
    char text[48];
    for (int i = 0; i < run_.segment_count(); ++i) {
        meter_text(text, sizeof text, run_, i);
        nnh::debug::print(kBottom, 12, static_cast<int>(y + 24 + i * line), text);
    }
    const float below = y + 24 + static_cast<float>(run_.segment_count()) * line;
    nnh::debug::printf(kBottom, 12, static_cast<int>(below), "%d frames, checksum %08X", run_.total_frame(),
                       static_cast<unsigned>(run_.checksum()));
    nnh::debug::print(kBottom, 12, static_cast<int>(below + line), "The PC's checksum must be the same.");
}

void Replay::draw_banner() const {
    if (!finished_) return;
    namespace gfx = nnh::gfx;
    const bool pass = all_ok(run_);
    const char* verdict = pass ? "REPLAY: PASS" : "REPLAY: FAIL";
    const nnh::Vec2 size = gfx::measure_text(verdict, 3);
    const float x = (400 - size.x) / 2;
    gfx::fill_rect(kTop, {x - 10, 6, size.x + 20, size.y + 12}, nnh::rgba(0, 0, 0, 190),
                   {.layer = 6, .ignore_camera = true});
    gfx::draw_text(kTop, {x, 12}, verdict,
                   {.color = pass ? kPass : kFail, .scale = 3, .layer = 6, .ignore_camera = true});
}
