#include "nnh/app.hpp"

#include "core/input_state.hpp"
#include "core/state.hpp"
#include "nnh/gfx.hpp"
#include "nnh/input.hpp"
#include "platform/platform.hpp"

namespace nnh {
namespace {

bool g_initialized = false;
bool g_frame_open = false;
std::uint64_t g_frame_count = 0;
Color g_clear[2] = {colors::black, colors::black};
detail::InputState g_input;

int screen_index(Screen screen) { return screen == Screen::Top ? 0 : 1; }

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
    g_input = {};
    return true;
}

bool frame() {
    if (!g_initialized) return false;
    close_frame();
    if (!platform::poll_events()) return false;
    g_input.update(platform::read_buttons());
    platform::begin_frame(g_clear[0], g_clear[1]);
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

void gfx::set_clear_color(Screen screen, Color color) { g_clear[screen_index(screen)] = color; }

bool input::held(Button button) { return (g_input.held & static_cast<std::uint32_t>(button)) != 0; }
bool input::pressed(Button button) { return (g_input.pressed() & static_cast<std::uint32_t>(button)) != 0; }
bool input::released(Button button) { return (g_input.released() & static_cast<std::uint32_t>(button)) != 0; }
std::uint32_t input::held_mask() { return g_input.held; }

} // namespace nnh
