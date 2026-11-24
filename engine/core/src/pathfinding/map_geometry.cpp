// Cells and world points (spec 025 FR-8): the centre of an agent's footprint, the anchor of an agent centred at a
// point (spec 012's half-open rule through collision::detail::first_cell), cells_to_points. The module's only floats
// with the follower; built with -ffp-contract=off.

#include "collision/grid_internal.hpp"
#include "pathfinding/path_block.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {
namespace {

Vec2 center_of(const detail::PathMapBlock& b, CellPos c) {
    const float half = static_cast<float>(b.agent) * 0.5f;
    return {b.origin.x + static_cast<float>(b.grid_x + c.x) * b.cell_w + half * b.cell_w,
            b.origin.y + static_cast<float>(b.grid_y + c.y) * b.cell_h + half * b.cell_h};
}

} // namespace

Vec2 PathMap::cell_center(CellPos cell) const {
    if (block_ == nullptr) {
        detail::path_misuse("PathMap::cell_center on an empty map");
        return {};
    }
    return center_of(*block_, cell);
}

CellPos PathMap::cell_at(Vec2 world) const {
    if (block_ == nullptr) {
        detail::path_misuse("PathMap::cell_at on an empty map");
        return {-1, -1};
    }
    if (!detail::path_point_ok(world, "PathMap::cell_at")) return {-1, -1};
    const detail::PathMapBlock& b = *block_;
    // The agent's top-left cell holds the point (k - 1) / 2 cells up and left of its centre (k = 1: the point).
    const float back = static_cast<float>(b.agent - 1) * 0.5f;
    const int gx = collision::detail::first_cell(b.origin.x, b.cell_w, world.x - back * b.cell_w);
    const int gy = collision::detail::first_cell(b.origin.y, b.cell_h, world.y - back * b.cell_h);
    return {gx - b.grid_x, gy - b.grid_y};
}

int cells_to_points(const PathMap& map, const CellPos* cells, int count, Vec2* out, int capacity) {
    const detail::PathMapBlock* b = detail::PathAccess::map(map);
    if (b == nullptr) {
        detail::path_misuse("cells_to_points on an empty map");
        return 0;
    }
    if (!detail::path_buffer_ok(cells, count, "cells_to_points (input)") ||
        !detail::path_buffer_ok(out, capacity, "cells_to_points")) {
        return 0;
    }
    const int n = count < capacity ? count : capacity;
    for (int i = 0; i < n; ++i) out[i] = center_of(*b, cells[i]);
    return count;
}

} // namespace nnh
