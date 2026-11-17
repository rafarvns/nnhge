// Engine lifecycle and the frame loop (nnh/app.hpp).

#include "nnh/app.hpp"

#include "core/engine_state.hpp"
#include "gfx/gfx_internal.hpp"
#include "input/input_internal.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/input.hpp"
#include "nnh/platform/video.hpp"

namespace nnh {
namespace {

bool g_initialized = false;
bool g_frame_open = false;
std::uint64_t g_frame_count = 0;

void close_frame() {
    if (g_frame_open) {
        platform::end_frame();
        g_frame_open = false;
    }
}

} // namespace

bool detail::frame_open() { return g_frame_open; }

bool init(const AppConfig& config) {
    if (g_initialized) return true;
    if (!platform::init(config)) return false;
    g_initialized = true;
    g_frame_count = 0;
    detail::input_reset();
    return true;
}

bool frame() {
    if (!g_initialized) return false;
    close_frame();
    if (!platform::poll_events()) return false;
    detail::input_update(platform::read_buttons());
    platform::begin_frame(detail::clear_color(Screen::Top), detail::clear_color(Screen::Bottom));
    g_frame_open = true;
    ++g_frame_count;
    return true;
}

void shutdown() {
    if (!g_initialized) return;
    close_frame();
    platform::shutdown();
    g_initialized = false;
}

std::uint64_t frame_count() { return g_frame_count; }

} // namespace nnh
