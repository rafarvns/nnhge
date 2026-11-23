// A grid and a box view together, and line of sight (spec 019 FR-12): the nearest hit of both, the cell winning a
// tie at one fraction.

#include "collision/query_check.hpp"
#include "collision/ray_hit.hpp"

namespace nnh::collision {

namespace {

/// Misuse makes line_of_sight false (Release): "no answer" never claims a clear view.
bool sight_inputs_ok(const Grid& grid, Vec2 from, Vec2 to) {
    return detail::grid_ok(grid, "line_of_sight") && detail::point_ok(from, "line_of_sight") &&
           detail::point_ok(to, "line_of_sight");
}

} // namespace

bool raycast(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, RayHit& hit, const QueryFilter& filter) {
    if (!detail::view_ok(boxes, "raycast") || !detail::grid_ok(grid, "raycast")) return false; // no half answers
    RayHit cell_hit, box_hit;
    const bool cell = raycast(grid, from, to, cell_hit, filter);
    const bool box = raycast(boxes, from, to, box_hit, filter);
    if (!cell && !box) return false;
    hit = cell && (!box || !(box_hit.fraction < cell_hit.fraction)) ? cell_hit : box_hit;
    return true;
}

bool line_of_sight(const Grid& grid, const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter) {
    if (!sight_inputs_ok(grid, from, to) || !detail::view_ok(boxes, "line_of_sight")) return false;
    QueryFilter f = filter;
    f.hit_from_inside = false; // what contains the viewer never blocks its view
    RayHit hit;
    return !raycast(grid, from, to, hit, f) && !detail::any_box_on_ray(boxes, from, to, f);
}

bool line_of_sight(const Grid& grid, Vec2 from, Vec2 to, const QueryFilter& filter) {
    if (!sight_inputs_ok(grid, from, to)) return false;
    QueryFilter f = filter;
    f.hit_from_inside = false;
    RayHit hit;
    return !raycast(grid, from, to, hit, f);
}

} // namespace nnh::collision
