#include "debug_view.hpp"

namespace debug_view {
namespace {

nnh::Rect cell_rect(const Surface& s, int x, int y) {
    return {s.origin.x + static_cast<float>(x) * s.size, s.origin.y + static_cast<float>(y) * s.size, s.size, s.size};
}

} // namespace

void draw_search(const Surface& s, const nnh::PathSearcher& searcher, const nnh::CellPos* path, int count,
                 const nnh::Vec2* waypoints, int waypoint_count, float world_to_surface) {
    namespace gfx = nnh::gfx;
    for (int y = s.y0; y <= s.y1; ++y) {
        for (int x = s.x0; x <= s.x1; ++x) {
            if (searcher.expanded({x, y})) {
                gfx::fill_rect(s.screen, cell_rect(s, x, y), nnh::rgba(250, 210, 80, 70), {.layer = 2});
            } else if (searcher.reached({x, y})) {
                gfx::draw_rect(s.screen, cell_rect(s, x, y), nnh::rgba(120, 220, 255, 160), {.layer = 2});
            }
        }
    }
    const float dot = s.size >= 8 ? 4 : 2;
    for (int i = 0; i < count; ++i) {
        const nnh::Rect r = cell_rect(s, path[i].x, path[i].y);
        gfx::fill_rect(s.screen, {r.x + (r.w - dot) / 2, r.y + (r.h - dot) / 2, dot, dot}, nnh::rgb(255, 255, 255),
                       {.layer = 3});
    }
    for (int i = 1; i < waypoint_count; ++i) {
        const nnh::Vec2 a = s.origin + waypoints[i - 1] * world_to_surface;
        const nnh::Vec2 b = s.origin + waypoints[i] * world_to_surface;
        gfx::draw_line(s.screen, a, b, nnh::rgb(255, 90, 200), {.layer = 3});
    }
}

void draw_field(const Surface& s, const nnh::DistanceField& field) {
    if (!field.ready()) return;
    for (int y = s.y0; y <= s.y1; ++y) {
        for (int x = s.x0; x <= s.x1; ++x) {
            const std::uint32_t d = field.distance({x, y});
            if (d == nnh::kUnreachable) continue;
            const int t = d > 600 ? 255 : static_cast<int>(d * 255 / 600);
            const nnh::Color c = nnh::rgba(255 - t / 3, 230 - t * 8 / 10, 40, 110);
            nnh::gfx::fill_rect(s.screen, cell_rect(s, x, y), c, {.layer = 2});
        }
    }
}

const char* mode_name(Mode mode) {
    switch (mode) {
    case Mode::Search: return "search";
    case Mode::Field: return "field";
    default: return "off";
    }
}

} // namespace debug_view
