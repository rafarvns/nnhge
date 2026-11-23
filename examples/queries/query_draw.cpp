#include "query_draw.hpp"

namespace query_draw {

namespace gfx = nnh::gfx;
using nnh::collision::Cell;

void ray(nnh::Screen screen, nnh::Vec2 from, nnh::Vec2 to, nnh::Color color, int layer) {
    gfx::draw_line(screen, from, to, color, {.layer = layer});
    gfx::fill_rect(screen, {from.x - 2, from.y - 2, 5, 5}, color, {.layer = layer});
}

void cells(nnh::Screen screen, const nnh::collision::Grid& grid, const nnh::collision::CellCrossing* crossed,
           int count, int layer) {
    for (int i = 0; i < count; ++i) {
        const nnh::collision::CellCrossing& c = crossed[i];
        const nnh::Rect r{grid.origin.x + static_cast<float>(c.x) * grid.cell_w,
                          grid.origin.y + static_cast<float>(c.y) * grid.cell_h, grid.cell_w, grid.cell_h};
        const nnh::Color shade = c.kind == Cell::Empty ? nnh::rgba(255, 255, 255, 40) : nnh::rgba(255, 120, 0, 90);
        gfx::fill_rect(screen, r, shade, {.layer = layer});
    }
}

void hit(nnh::Screen screen, const nnh::collision::RayHit& h, bool first, int layer) {
    const nnh::Color color = first ? nnh::colors::yellow : nnh::colors::white;
    const float half = first ? 3.0f : 2.0f;
    gfx::fill_rect(screen, {h.point.x - half, h.point.y - half, 2 * half + 1, 2 * half + 1}, color, {.layer = layer});
    gfx::draw_line(screen, h.point, h.point + h.normal * 8.0f, nnh::rgb(0, 230, 230), {.layer = layer});
}

void cast_box(nnh::Screen screen, nnh::Vec2 top_left, nnh::Vec2 size, nnh::Color color, int layer) {
    gfx::draw_rect(screen, {top_left.x, top_left.y, size.x, size.y}, color, {.layer = layer});
}

} // namespace query_draw
