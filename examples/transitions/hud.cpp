#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/transition.hpp>

namespace tr = nnh::transition;

namespace {

const char* phase_name(tr::Phase p) {
    switch (p) {
    case tr::Phase::Out: return "out";
    case tr::Phase::Covered: return "covered";
    case tr::Phase::In: return "in";
    case tr::Phase::None: break;
    }
    return "-";
}

} // namespace

void draw_hud(const Settings& s, int room, nnh::Vec2 square) {
    namespace gfx = nnh::gfx;
    const nnh::Screen bottom = nnh::Screen::Bottom;
    // The room's map: the top screen at a quarter scale, the square on it.
    gfx::fill_rect(bottom, {8, 8, 100, 60}, room == 0 ? nnh::rgb(110, 100, 86) : nnh::rgb(70, 120, 150));
    gfx::draw_rect(bottom, {8, 8, 100, 60}, nnh::colors::white);
    gfx::fill_rect(bottom, {8 + square.x / 4, 8 + square.y / 4, 4, 4}, nnh::rgb(60, 210, 230));
    char text[256];
    std::snprintf(text, sizeof text,
                  "room %d\neffect  %s\nreveal  %s\nscreens %s\n\nphase   %s\ncover   %.2f / %.2f\nloading %.2f\n"
                  "frames  %d",
                  room + 1, fx_name(s.fx), s.continue_reveal ? "continue" : "reverse", screens_name(s.screens),
                  phase_name(tr::phase()), static_cast<double>(tr::coverage(nnh::Screen::Top)),
                  static_cast<double>(tr::coverage(bottom)), static_cast<double>(tr::loading_progress()),
                  tr::frames());
    gfx::draw_text(bottom, {120, 8}, text);
    gfx::draw_text(bottom, {8, 150},
                   "LEFT/RIGHT effect  L reveal  R screens\nA room  B loading step  X loading scene\n"
                   "touch: iris centre\nSELECT check  Y bench  START quit",
                   {.color = nnh::rgb(180, 180, 200)});
}
