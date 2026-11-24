// Smoothing (spec 025 FR-15): from each kept cell, the furthest cell of the first run of path cells a straight
// centre-to-centre segment reaches — every cell it crosses (after the anchor) walkable and no heavier than the
// heaviest path cell it replaces, every corner it passes allowed by the map's rule (Never: as OnlyIfNoObstacles;
// Always: not read). Greedy: bounded by the sum of each run's walks. In place is safe: waypoint k is written after
// path cells 0..j (j >= k) were read, and later reads are past the anchor.

#include "pathfinding/path_block.hpp"
#include "pathfinding/path_check.hpp"
#include "pathfinding/smooth_walk.hpp"

namespace nnh {
namespace {

struct Bytes {
    const std::uint8_t* cells;
    int stride;
    std::uint32_t at(int x, int y) const { return cells[detail::padded_index(stride, x, y)]; }
};

/// The corner rule on the side cells; Never keeps diagonals out entirely, so both must be open.
bool corner_ok(Diagonals mode, std::uint32_t a, std::uint32_t b) {
    if (mode == Diagonals::Always) return true;
    if (mode == Diagonals::AtLeastOneWalkable) return (a | b) != 0;
    return a != 0 && b != 0;
}

bool visible(const Bytes& m, Diagonals mode, CellPos a, CellPos b, std::uint32_t heaviest) {
    detail::SmoothWalk walk(a, b);
    detail::WalkStep s;
    while (walk.next(s)) {
        if (s.corner && !corner_ok(mode, m.at(s.x, s.y - s.sy), m.at(s.x - s.sx, s.y))) return false;
        const std::uint32_t w = m.at(s.x, s.y);
        if (w == 0 || w > heaviest) return false;
    }
    return true;
}

} // namespace

int smooth_path(const PathMap& map, const CellPos* path, int count, CellPos* out, int capacity) {
    const detail::PathMapBlock* b = detail::PathAccess::map(map);
    if (b == nullptr) {
        detail::path_misuse("smooth_path on an empty map");
        return 0;
    }
    if (!detail::path_buffer_ok(path, count, "smooth_path (input)") ||
        !detail::path_buffer_ok(out, capacity, "smooth_path")) {
        return 0;
    }
    for (int i = 0; i < count; ++i) {
        if (!detail::cell_ok(b->width, b->height, path[i].x, path[i].y, "smooth_path")) return 0;
    }
    if (count == 0) return 0;
    const Bytes m{b->cells, b->stride};
    int total = 0;
    const auto emit = [&](CellPos c) {
        if (total < capacity) out[total] = c;
        ++total;
    };
    CellPos anchor = path[0];
    emit(anchor);
    int a = 0;
    while (a < count - 1) {
        int j = a + 1;
        std::uint32_t heaviest = m.at(path[j].x, path[j].y);
        while (j + 1 < count) {
            const CellPos next = path[j + 1];
            const std::uint32_t w = m.at(next.x, next.y);
            const std::uint32_t h = w > heaviest ? w : heaviest;
            if (!visible(m, b->diagonals, anchor, next, h)) break;
            heaviest = h;
            ++j;
        }
        anchor = path[j];
        emit(anchor);
        a = j;
    }
    return total;
}

} // namespace nnh
