#include "stick_view.hpp"

#include <nnh/debug.hpp>

namespace {

/// Prints one character centered on (x, y) (the font is about 8 pixels wide).
void mark(nnh::Screen screen, int x, int y, const char* text, nnh::Color color) {
    nnh::debug::print(screen, x - 4, y - nnh::debug::line_height() / 2, text, color);
}

} // namespace

void draw_stick(nnh::Screen screen, int cx, int cy, const char* label, nnh::Stick stick, bool available) {
    const int r = kStickRadius;
    const int line = nnh::debug::line_height();
    nnh::debug::print(screen, cx - r, cy - r - line - 4, label, nnh::colors::yellow);
    if (!available) {
        nnh::debug::print(screen, cx - r, cy, "not on this system", nnh::colors::gray);
        return;
    }
    // Box corners and edge midpoints, and the center.
    for (int dy = -r; dy <= r; dy += r) {
        for (int dx = -r; dx <= r; dx += r) {
            mark(screen, cx + dx, cy + dy, (dx == 0 && dy == 0) ? "+" : ".", nnh::colors::gray);
        }
    }
    // Stick y grows up, screen y grows down.
    const int dot_x = cx + static_cast<int>(stick.x * static_cast<float>(r));
    const int dot_y = cy - static_cast<int>(stick.y * static_cast<float>(r));
    mark(screen, dot_x, dot_y, "O", nnh::colors::green);
    nnh::debug::printf(screen, cx - r, cy + r + 6, "x %+.2f", static_cast<double>(stick.x));
    nnh::debug::printf(screen, cx - r, cy + r + 6 + line, "y %+.2f", static_cast<double>(stick.y));
}
