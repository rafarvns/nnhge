// Engine lifecycle and the frame loop (nnh/app.hpp).

#include "nnh/app.hpp"

#include "app/data_folder.hpp"
#include "assets/assets_internal.hpp"
#include "audio/audio_internal.hpp"
#include "core/engine_state.hpp"
#include "core/input_hooks.hpp"
#include "core/shutdown_hooks.hpp"
#include "crash/crash_internal.hpp"
#include "debug/dev_tools.hpp"
#include "events/events_internal.hpp"
#include "scene/scenes_internal.hpp"
#include "nnh/input.hpp"
#include "nnh/platform/system.hpp"
#include "nnh/time.hpp"
#include "gfx/gfx_internal.hpp"
#include "input/input_internal.hpp"
#include "log/engine_channels.hpp"
#include "log/file_sink.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/platform/time.hpp"
#include "nnh/platform/video.hpp"
#include "remote/remote_internal.hpp"
#include "time/time_internal.hpp"

namespace nnh {
namespace {

bool g_initialized = false;
bool g_frame_open = false;
std::uint64_t g_frame_count = 0;
char g_data_folder[48];

/// Ends the open frame: 2D passes per screen, then the debug pass (text, overlay) on top, then present.
void close_frame() {
    if (g_frame_open) {
        detail::gfx_submit();
        detail::overlay_draw();     // recorded after the game's debug text, so it covers it
        detail::debug_text_flush(); // the debug pass
        platform::end_frame();
        if constexpr (build::kDevToolsEnabled) detail::remote_frame_ended(g_frame_count); // screenshots
        g_frame_open = false;
        detail::gfx_after_frame(); // textures released during the frame are no longer in use
    }
}

} // namespace

void (*detail::g_run_input_hooks)() = nullptr; // D32: one pointer, set while a module has after-input hooks

bool detail::frame_open() { return g_frame_open; }
bool detail::engine_initialized() { return g_initialized; }

bool init(const AppConfig& config) {
    if (g_initialized) return true;
    // Platforms always receive a sanitized data folder name.
    AppConfig resolved = config;
    detail::derive_data_folder(config.data_folder != nullptr ? config.data_folder : config.title, g_data_folder,
                               sizeof g_data_folder);
    resolved.data_folder = g_data_folder;
    if (!platform::init(resolved)) {
        NNH_LOG_ERROR(app, "platform init failed: %s", platform::last_error());
        return false;
    }
    if (!detail::events_init(config.events)) {
        platform::shutdown();
        return false;
    }
    detail::assets_init(); // before gfx: textures may load from assets
    if (!detail::gfx_init(config.gfx)) {
        detail::assets_shutdown();
        detail::events_shutdown();
        platform::shutdown();
        return false;
    }
    if (!detail::audio_init(config.audio)) { // after assets: sounds load from them
        detail::gfx_shutdown();
        detail::assets_shutdown();
        detail::events_shutdown();
        platform::shutdown();
        return false;
    }
    g_initialized = true;
    g_frame_count = 0;
    detail::input_reset();
    detail::time_reset(platform::ticks_ns());
    detail::profile_reset();
    detail::overlay_reset();
    detail::debug_text_clear();
    detail::crash_install();
    if constexpr (build::kDevToolsEnabled) detail::remote_reset();
    detail::log_file_start();
    NNH_LOG_INFO(app, "started \"%s\" (%s build), data in %s", config.title, build::flavor_name(),
                 platform::fs_root());
    return true;
}

bool frame() {
    if (!g_initialized) return false;
    close_frame();
    detail::scenes_apply_pending(); // changes requested during the frame that just ended
    if (!platform::poll_events()) return false;
    detail::time_frame_started(platform::ticks_ns());
    platform::InputSample input_sample;
    platform::read_input(input_sample);
    if constexpr (build::kDevToolsEnabled) {
        // The PC's input enters before the frame's input is read (spec 013): queries and events see it.
        if (!detail::remote_frame_start(g_frame_count + 1, input_sample)) return false;
    }
    detail::input_update(input_sample);
    detail::profile_frame_boundary();
    detail::overlay_update(time::delta(), input::held_mask(), detail::input_pressed_mask(),
                           platform::poll_dev_requests());
    platform::begin_frame(detail::clear_color(Screen::Top), detail::clear_color(Screen::Bottom));
    g_frame_open = true;
    ++g_frame_count;
    detail::input_publish_events();
    if (detail::g_run_input_hooks != nullptr) detail::g_run_input_hooks(); // D32: input actions, before dispatch
    detail::audio_frame(time::delta()); // ended voices -> SoundFinished
    detail::events_dispatch(); // after input, before the game's update
    return true;
}

void shutdown() {
    if (!g_initialized) return;
    close_frame();
    NNH_LOG_INFO(app, "shutdown after %llu frames", static_cast<unsigned long long>(g_frame_count));
    detail::scenes_shutdown(); // on_exit() may still unsubscribe: before the bus goes away
    detail::events_shutdown();
    detail::gfx_shutdown(); // after scenes: on_exit() may still release textures
    detail::audio_shutdown(); // after scenes too (sounds); before assets
    // Optional modules (data assets) free what they keep, before the leak report. Release has no leak report and
    // skips this, so games that don't use those modules stay byte-for-byte the same (spec 014 AC-6).
    if constexpr (build::kDevToolsEnabled) detail::run_shutdown_hooks();
    detail::assets_shutdown();
    if constexpr (build::kDevToolsEnabled) mem::report_leaks();
    detail::log_file_stop();
    if constexpr (build::kDevToolsEnabled) detail::remote_shutdown(); // before the platform closes the socket
    platform::shutdown();
    g_initialized = false;
}

std::uint64_t frame_count() { return g_frame_count; }

} // namespace nnh
