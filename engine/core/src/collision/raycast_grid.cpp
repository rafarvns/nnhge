// Rays against a grid (spec 019 FR-8, FR-9, FR-11): the grid walk, one cell lookup per visited cell, and the
// blocking rules — Solid blocks when entered; OneWay only through its top face going down (normal (0, −1), also
// at a corner); a start inside a Solid cell only with hit_from_inside, inside a OneWay cell never.

#include "collision/grid_internal.hpp"
#include "collision/grid_walk.hpp"
#include "collision/hit_list.hpp"
#include "collision/query_check.hpp"
#include "collision/ray_hit.hpp"

namespace nnh::collision {

namespace {

/// The outside can't block this ray: the walk may skip it (clip to the grid).
bool outside_inert(const Grid& grid, const detail::Segment& s, const QueryFilter& filter) {
    if (grid.outside == Cell::Solid) return false;
    return grid.outside == Cell::Empty || filter.ignore_one_way || s.sy <= 0;
}

/// Cell `c` (of kind `kind`) as a hit, or false when it doesn't block.
bool cell_hit(const detail::GridWalk& walk, const detail::Segment& s, const detail::WalkCell& c, Cell kind,
              Vec2 from, const QueryFilter& filter, RayHit& hit) {
    if (kind == Cell::Empty) return false;
    if (c.inside) {
        if (kind != Cell::Solid || !filter.hit_from_inside) return false;
        hit = detail::inside_hit(from);
    } else {
        const bool one_way = kind == Cell::OneWay;
        if (one_way && (!c.face_y || s.sy <= 0 || filter.ignore_one_way)) return false;
        hit = {};
        hit.fraction = detail::to_fraction(c.t);
        hit.normal = one_way ? Vec2{0, -1} : detail::face_normal(s, c.face_x);
        const float x0 = walk.edge_x(c.x), x1 = walk.edge_x(c.x + 1);
        const float y0 = walk.edge_y(c.y), y1 = walk.edge_y(c.y + 1);
        if (c.corner) {
            hit.point = c.corner_point;
        } else if (c.face_x) {
            hit.point = {s.sx > 0 ? x0 : x1, detail::clamp_to(detail::along(s.py, s.dy, c.t), y0, y1)};
        } else {
            hit.point = {detail::clamp_to(detail::along(s.px, s.dx, c.t), x0, x1), s.sy > 0 ? y0 : y1};
        }
    }
    hit.cell = {c.x, c.y, kind};
    return true;
}

bool inputs_ok(const Grid& grid, Vec2 from, Vec2 to, const char* who) {
    return detail::grid_ok(grid, who) && detail::point_ok(from, who) && detail::point_ok(to, who);
}

} // namespace

namespace detail {

bool first_cell_hit(const Grid& grid, const Segment& s, RayHit& hit, const QueryFilter& filter) {
    GridWalk walk(grid, s, outside_inert(grid, s, filter));
    while (const WalkCell* c = walk.step()) {
        const Cell kind = cell_at(grid, c->x, c->y);
        if (kind != Cell::Empty && cell_hit(walk, s, *c, kind, s.from, filter, hit)) {
            hit.distance = hit.fraction * ray_length(s.from, s.to);
            return true;
        }
    }
    return false;
}

} // namespace detail

bool raycast(const Grid& grid, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    if (!inputs_ok(grid, from, to, "raycast")) return false;
    return detail::first_cell_hit(grid, detail::make_segment(from, to), hit, filter);
}

int raycast_all(const Grid& grid, Vec2 from, Vec2 to, RayHit* hits, int capacity, const QueryFilter& filter) {
    if (!inputs_ok(grid, from, to, "raycast_all") || !detail::buffer_ok(hits, capacity, "raycast_all")) return 0;
    const detail::Segment s = detail::make_segment(from, to);
    detail::GridWalk walk(grid, s, outside_inert(grid, s, filter));
    RayHit hit;
    float length = -1;
    int total = 0;
    while (const detail::WalkCell* c = walk.step()) {
        const Cell kind = detail::cell_at(grid, c->x, c->y);
        if (kind == Cell::Empty || !cell_hit(walk, s, *c, kind, from, filter, hit)) continue;
        if (length < 0) length = detail::ray_length(from, to);
        hit.distance = hit.fraction * length;
        total = detail::insert_hit(hits, capacity, total, hit); // walk order is fraction order
    }
    return total;
}

} // namespace nnh::collision
