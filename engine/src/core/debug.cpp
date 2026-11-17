#include "nnh/debug.hpp"

#include <cstdarg>
#include <cstdio>

#include "core/state.hpp"
#include "platform/platform.hpp"

namespace nnh::debug {

void print(Screen screen, int x, int y, const char* text, Color color) {
    if (!detail::frame_open() || text == nullptr) return;
    platform::debug_text(screen, x, y, text, color);
}

void printf(Screen screen, int x, int y, const char* format, ...) {
    if (!detail::frame_open() || format == nullptr) return;
    char buffer[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    platform::debug_text(screen, x, y, buffer, colors::white);
}

int line_height() { return platform::debug_line_height(); }

} // namespace nnh::debug
