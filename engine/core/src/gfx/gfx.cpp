// Per-frame graphics state (nnh/gfx.hpp).

#include "nnh/gfx.hpp"

#include "gfx/gfx_internal.hpp"

namespace nnh {
namespace {

Color g_clear[2] = {colors::black, colors::black};

int screen_index(Screen screen) { return screen == Screen::Top ? 0 : 1; }

} // namespace

void gfx::set_clear_color(Screen screen, Color color) { g_clear[screen_index(screen)] = color; }

Color detail::clear_color(Screen screen) { return g_clear[screen_index(screen)]; }

} // namespace nnh
