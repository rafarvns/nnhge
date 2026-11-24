#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

void draw_hud(const Level& level, const Hero& hero, const Crowd& crowd, debug_view::Mode mode) {
    namespace gfx = nnh::gfx;
    char line[96];
    const nnh::PathResult r = hero.last();
    if (hero.has_goal()) {
        std::snprintf(line, sizeof line, "%s  cost %u  cells %d  expanded %d  %.2f ms", status_name(r.status),
                      static_cast<unsigned>(r.cost), r.cells, r.expanded, hero.last_ms());
    } else {
        std::snprintf(line, sizeof line, "touch the map below to set a goal");
    }
    gfx::fill_rect(nnh::Screen::Top, {0, 0, 400, 12}, nnh::rgba(0, 0, 0, 170), {.layer = 8, .ignore_camera = true});
    gfx::draw_text(nnh::Screen::Top, {4, 2}, line, {.layer = 9, .ignore_camera = true});
    int walkable = 0;
    for (int y = 0; y < level.height(); ++y) {
        for (int x = 0; x < level.width(); ++x) walkable += level.small.blocked(x, y) ? 0 : 1;
    }
    std::snprintf(line, sizeof line, "field %d/%d%s  %s  %s", crowd.field().settled(), walkable,
                  crowd.field().building() ? " (building)" : "", hero.big() ? "2x2" : "1x1",
                  hero.paused() ? "paused" : "");
    gfx::draw_text(nnh::Screen::Bottom, {4, 4}, line, {.color = nnh::rgb(220, 220, 150)});
    static const char* const kShort[4] = {"4 dirs", "8 always", "8 one side", "8 no cuts"};
    std::snprintf(line, sizeof line, "%s  smooth %s  view %s", kShort[static_cast<int>(level.small.diagonals())],
                  hero.smoothing() ? "on" : "off", debug_view::mode_name(mode));
    gfx::draw_text(nnh::Screen::Bottom, {4, 16}, line, {.color = nnh::rgb(200, 210, 230)});
    gfx::draw_text(nnh::Screen::Bottom, {4, 214},
                   "touch goal A door X size L mode R smooth\nUp view Down pause SELECT check Y bench",
                   {.color = nnh::rgb(150, 160, 190)});
}
