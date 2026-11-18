// Debug text (nnh/debug.hpp). Recorded during the frame, drawn on top of everything at its end.

#include "nnh/debug.hpp"

#include <cstdarg>
#include <cstdio>

#include "core/engine_state.hpp"
#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "nnh/platform/video.hpp"

namespace nnh {
namespace {

constexpr int kMaxTexts = 256;
constexpr std::size_t kTextBytes = 8 * 1024;

detail::DebugText g_entries[kMaxTexts];
char g_text[kTextBytes];
detail::DebugList g_list = [] {
    detail::DebugList list;
    list.reset(g_entries, kMaxTexts, g_text, kTextBytes);
    return list;
}();

} // namespace

void detail::debug_text_flush() {
    for (int i = 0; i < g_list.count(); ++i) {
        const DebugText& t = g_list.at(i);
        platform::debug_text(t.screen, t.x, t.y, g_list.text(i), t.color);
    }
    g_list.clear();
}

void detail::debug_text_clear() { g_list.clear(); }

void debug::print(Screen screen, int x, int y, const char* text, Color color) {
    if (!detail::frame_open() || text == nullptr) return;
    g_list.add(screen, x, y, text, color);
}

void debug::printf(Screen screen, int x, int y, const char* format, ...) {
    if (!detail::frame_open() || format == nullptr) return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    g_list.add(screen, x, y, buffer, colors::white);
}

int debug::line_height() { return platform::debug_line_height(); }

} // namespace nnh
