// Pause and step inside nnh::frame() (dev_pause.hpp, D52). Debug/Development only: app.cpp reaches it from discarded
// branches in Release. A paused tick polls events and input, runs the remote session for the next game frame's
// messages, takes the engine controls, then draws the held frame again under the live overlay.

#include "app/dev_pause.hpp"

#include "debug/dev_controls.hpp"
#include "debug/dev_tools.hpp"
#include "debug/live_hook.hpp"
#include "debug/overlay_pages.hpp"
#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_replay.hpp"
#include "log/engine_channels.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/platform/time.hpp"
#include "nnh/platform/video.hpp"
#include "remote/remote_internal.hpp"
#include "time/time_internal.hpp"

namespace nnh::detail {

bool g_dev_scenes_changed = false;
std::uint32_t g_dev_frame_requests = 0;
bool g_time_keep_delta = false;

void audio_hold(bool hold); // audio/audio_hold.cpp

namespace {

PauseState g_pause;
DevControls g_controls;
bool g_pause_requested = false;

void apply_requests(std::uint32_t requests) {
    if ((requests & platform::kDevHoldAtStart) != 0) g_pause.hold();
    if ((requests & platform::kDevTogglePause) != 0) g_pause.toggle();
    if ((requests & platform::kDevStepFrame) != 0) g_pause.step(1);
    if (g_pause_requested) {
        g_pause_requested = false;
        g_pause.pause();
    }
}

/// What a paused tick shows where the held frame would be.
void draw_note(bool replayed) {
    if (replayed) return;
    const char* note = g_pause.held() ? "waiting for nnh (held start, 10 s)" : "frame not held: the scenes changed";
    debug_record_text(Screen::Top, 4, 4, note, colors::yellow);
}

/// One paused tick. @return false when the game must quit.
bool paused_tick(std::uint64_t next_frame, platform::InputSample& sample) {
    if (!platform::poll_events()) return false;
    sample = platform::InputSample{};
    platform::read_input(sample);
    if (!remote_frame_start(next_frame, sample)) return false; // messages due at the next game frame
    const DevInput in = g_controls.update(sample);
    const std::uint32_t requests = platform::poll_dev_requests();
    apply_requests(requests);
    overlay_pages_input(in, requests, true);
    if (g_pause.tick(remote::session_attached())) NNH_LOG_WARN(app, "held start: no PC after 10 s, running");
    platform::begin_frame(clear_color(Screen::Top), clear_color(Screen::Bottom));
    gfx_gpu_idle();
    const bool replayed = gfx_replay(g_dev_scenes_changed);
    const auto frame = static_cast<std::uint32_t>(next_frame - 1);
    if (g_live_ops != nullptr) g_live_ops->frame_end(frame, true);
    debug_replay_game_entries();
    draw_note(replayed);
    overlay_draw();
    debug_text_flush();
    platform::end_frame();
    remote_frame_ended(next_frame); // screenshots asked for while paused show the paused display
    gfx_frame_submitted();
    return true;
}

} // namespace

PauseState& dev_pause_state() { return g_pause; }

void dev_request_pause() { g_pause_requested = true; }

void dev_pause_reset() {
    g_pause = PauseState{};
    g_controls.reset();
    g_pause_requested = false;
    g_dev_scenes_changed = false;
    g_time_keep_delta = false;
    g_gfx_held = false;
}

bool dev_frame_begin(std::uint64_t next_frame, platform::InputSample& sample, std::uint32_t dev_requests) {
    const DevInput in = g_controls.update(sample);
    apply_requests(dev_requests);
    overlay_pages_input(in, dev_requests, false);
    if (g_pause.begin_frame()) return true;
    audio_hold(true);
    bool running = true;
    while (running && !g_pause.begin_frame()) running = paused_tick(next_frame, sample);
    audio_hold(false);
    if (!running) return false;
    g_time_keep_delta = true; // this frame keeps the delta measured before the pause; the clock restarts now
    time_frame_started(platform::ticks_ns());
    return true;
}

} // namespace nnh::detail
