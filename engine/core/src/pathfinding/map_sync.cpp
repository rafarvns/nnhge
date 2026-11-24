// Syncing a path map from a collision::Grid (spec 025 FR-3): each grid cell read once through spec 012's cell rule
// (collision/grid_internal.hpp: `outside` beyond the grid), Empty walkable, Solid blocked, OneWay blocked unless the
// map says otherwise; weights from the game's callback, called once per walkable cell, row-major, only here.

#include "collision/grid_internal.hpp"
#include "pathfinding/footprint.hpp"
#include "pathfinding/path_block.hpp"
#include "pathfinding/path_check.hpp"

namespace nnh {
namespace {

std::uint8_t weight_from(const PathWeights& w, int gx, int gy) {
    const int v = w.weight(w.context, gx, gy);
    if (v == kPathBlocked) return 0;
    if (v >= 1 && v <= 255) return static_cast<std::uint8_t>(v);
    detail::path_misuse("PathMap::sync: the weight callback returned a value not 1..255 or kPathBlocked");
    return v < 1 ? 1 : 255; // Release: clamped
}

void sync_span(detail::PathMapBlock& b, const collision::Grid& grid, const detail::CellSpan& s,
               const PathWeights& w) {
    for (int y = s.y0; y <= s.y1; ++y) {
        const int gy = b.grid_y + y;
        for (int x = s.x0; x <= s.x1; ++x) {
            const int gx = b.grid_x + x;
            const collision::Cell c = collision::detail::cell_at(grid, gx, gy);
            std::uint8_t v = 1;
            if (c == collision::Cell::Solid || (c == collision::Cell::OneWay && !b.one_way_walkable)) {
                v = 0;
            } else if (w.weight != nullptr) {
                v = weight_from(w, gx, gy);
            }
            detail::write_cell(b, x, y, v);
        }
    }
    if (!s.empty()) detail::footprint_refresh(b, s.x0, s.y0, s.x1, s.y1);
    ++b.version;
}

} // namespace

void PathMap::sync(const collision::Grid& grid, const PathWeights& weights) {
    if (block_ == nullptr) {
        detail::path_misuse("PathMap::sync on an empty map");
        return;
    }
    sync_span(*block_, grid, {0, 0, block_->width - 1, block_->height - 1}, weights);
}

void PathMap::sync(const collision::Grid& grid, CellRect region, const PathWeights& weights) {
    if (block_ == nullptr) {
        detail::path_misuse("PathMap::sync on an empty map");
        return;
    }
    detail::CellSpan s;
    if (!detail::clip_region(region, block_->width, block_->height, s, "PathMap::sync")) return;
    sync_span(*block_, grid, s, weights);
}

} // namespace nnh
