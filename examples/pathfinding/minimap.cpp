#include "minimap.hpp"

namespace minimap {
namespace {

nnh::Color color_of(const Level& level, int x, int y) {
    const int wall = level.map.tile(level.walls, x, y).id;
    if (wall == level.door_tile) return nnh::rgb(160, 108, 58);
    if (wall == 8) return nnh::rgb(18, 44, 110); // deep water
    if (wall != 0) return nnh::rgb(120, 116, 126);
    switch (level.map.tile(level.ground, x, y).id) {
    case 3: return nnh::rgb(50, 110, 190);  // water
    case 4: return nnh::rgb(70, 84, 44);    // swamp
    case 5: return nnh::rgb(196, 170, 118); // road
    case 6: return nnh::rgb(110, 70, 38);   // doorway
    default: return nnh::rgb(58, 60, 72);   // floor
    }
}

bool same(nnh::Color a, nnh::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

} // namespace

bool touched_cell(const Level& level, int tx, int ty, nnh::CellPos& cell) {
    if (tx < kLeft || ty < kTop) return false;
    const int x = (tx - kLeft) / kCell;
    const int y = (ty - kTop) / kCell;
    if (x >= level.width() || y >= level.height()) return false;
    cell = {x, y};
    return true;
}

void draw_level(const Level& level) {
    for (int y = 0; y < level.height(); ++y) {
        int run = 0;
        nnh::Color c = color_of(level, 0, y);
        for (int x = 1; x <= level.width(); ++x) {
            const nnh::Color n = x < level.width() ? color_of(level, x, y) : nnh::Color{};
            if (x < level.width() && same(n, c)) continue;
            const nnh::Rect r = cell_rect(run, y);
            nnh::gfx::fill_rect(nnh::Screen::Bottom, {r.x, r.y, static_cast<float>((x - run) * kCell), r.h}, c);
            run = x;
            c = n;
        }
    }
}

} // namespace minimap
