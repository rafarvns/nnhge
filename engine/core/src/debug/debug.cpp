// Debug text (nnh/debug.hpp). Recorded during the frame and drawn with the built-in font in the debug
// pass at its end, on top of everything (debug_pass.cpp). Two lists alternate, so the previous frame's
// stays readable for tests.

#include "nnh/debug.hpp"

#include <cstdarg>
#include <cstdio>

#include "core/engine_state.hpp"
#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "gfx/gfx_replay.hpp"
#include "nnh/build.hpp"

namespace nnh {
namespace {

constexpr int kMaxEntries = 512;           // texts + overlay rectangles per frame
constexpr std::size_t kTextBytes = 8 * 1024;

detail::DebugText g_entries[2][kMaxEntries];
char g_text[2][kTextBytes];
detail::DebugList g_lists[2];
int g_current = 0;

detail::DebugList& current() {
    static bool ready = false;
    if (!ready) {
        for (int i = 0; i < 2; ++i) g_lists[i].reset(g_entries[i], kMaxEntries, g_text[i], kTextBytes);
        ready = true;
    }
    return g_lists[g_current];
}

} // namespace

void detail::debug_record_text(Screen screen, int x, int y, const char* text, Color color) {
    if constexpr (build::kDevToolsEnabled) { // counted for the held frame's replay (spec 030)
        if (current().add(screen, x, y, text, color)) ++g_debug_recorded;
    } else {
        current().add(screen, x, y, text, color);
    }
}

void detail::debug_record_rect(Screen screen, int x, int y, int w, int h, Color color) {
    if constexpr (build::kDevToolsEnabled) {
        if (current().add_rect(screen, x, y, w, h, color)) ++g_debug_recorded;
    } else {
        current().add_rect(screen, x, y, w, h, color);
    }
}

void detail::debug_text_flush() {
    debug_pass_submit(current());
    g_current ^= 1;
    current().clear();
    if constexpr (build::kDevToolsEnabled) g_debug_recorded = 0;
}

void detail::debug_text_clear() {
    current();
    g_lists[0].clear();
    g_lists[1].clear();
}

const detail::DebugList& detail::debug_list_last_frame() {
    current();
    return g_lists[g_current ^ 1];
}

void debug::print(Screen screen, int x, int y, const char* text, Color color) {
    if (!detail::frame_open() || text == nullptr) return;
    detail::debug_record_text(screen, x, y, text, color);
}

void debug::printf(Screen screen, int x, int y, const char* format, ...) {
    if (!detail::frame_open() || format == nullptr) return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    detail::debug_record_text(screen, x, y, buffer, colors::white);
}

int debug::line_height() { return kFontLineHeight; }

} // namespace nnh
