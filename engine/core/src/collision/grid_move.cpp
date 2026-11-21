// collision::move (spec 012 FR-8): x then y, each a sweep over every cell line the box enters, so no
// speed tunnels. Positions after a stop are snapped so the box's cell range excludes the blocking line
// even when the float sums round the wrong way (otherwise the next move would ignore that line).

#include <cmath>

#include "collision/grid_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh::collision {

namespace detail {

namespace {

// One axis seen as lines (columns for X, rows for Y), each crossed over the box's perpendicular range.
struct Lines {
    const Grid& grid;
    Axis axis;
    float origin;
    float size;
    int count;
    int across0, across1; // perpendicular cells of the box, clamped to [-1, count across]
    bool one_way;
    float start_end; // the box's far edge (bottom) before moving, for the one-way rule
};

float edge(const Lines& l, int i) { return l.origin + static_cast<float>(i) * l.size; }

int clamp_index(int i, int lo, int hi) { return i < lo ? lo : (i > hi ? hi : i); }

bool kind_blocks(const Lines& l, Cell kind, int i) {
    if (kind == Cell::Solid) return true;
    return kind == Cell::OneWay && l.one_way && edge(l, i) >= l.start_end;
}

bool outside_can_block(const Lines& l) {
    return l.grid.outside == Cell::Solid || (l.grid.outside == Cell::OneWay && l.one_way);
}

bool line_blocks(const Lines& l, int i) {
    if (i < 0 || i >= l.count) return kind_blocks(l, l.grid.outside, i);
    for (int j = l.across0; j <= l.across1; ++j) {
        const Cell kind = l.axis == Axis::X ? cell_at(l.grid, i, j) : cell_at(l.grid, j, i);
        if (kind_blocks(l, kind, i)) return true;
    }
    return false;
}

// First blocking line in s..e (s <= e), skipping runs of outside lines that cannot block.
bool find_forward(const Lines& l, int s, int e, int& hit) {
    const bool outside = outside_can_block(l);
    for (int i = s; i <= e; ++i) {
        if (!outside && i >= l.count) return false;
        if (!outside && i < 0) {
            i = -1; // next: line 0
            continue;
        }
        if (line_blocks(l, i)) {
            hit = i;
            return true;
        }
    }
    return false;
}

// First blocking line in s..e going down (s >= e).
bool find_backward(const Lines& l, int s, int e, int& hit) {
    const bool outside = outside_can_block(l);
    for (int i = s; i >= e; --i) {
        if (!outside && i < 0) return false;
        if (!outside && i >= l.count) {
            i = l.count; // next: the last line
            continue;
        }
        if (line_blocks(l, i)) {
            hit = i;
            return true;
        }
    }
    return false;
}

float ulp_scale(float a, float b, float c) {
    float m = std::fabs(a);
    if (std::fabs(b) > m) m = std::fabs(b);
    if (c > m) m = c;
    return std::nextafter(m, INFINITY) - m;
}

// Highest start whose far end stays out of line `i` (the end is exclusive).
float place_before(const Lines& l, int i, float len) {
    const float wall = edge(l, i);
    float pos = wall - len;
    float step = ulp_scale(wall, pos, len);
    for (int k = 0; k < 64 && last_cell(l.origin, l.size, pos + len) >= i; ++k, step *= 2) pos -= step;
    return pos;
}

// Lowest start that stays out of line `i`.
float place_after(const Lines& l, int i) {
    float pos = edge(l, i + 1);
    float step = ulp_scale(pos, l.size, 0);
    for (int k = 0; k < 64 && first_cell(l.origin, l.size, pos) <= i; ++k, step *= 2) pos += step;
    return pos;
}

} // namespace

SweepResult sweep(const Grid& grid, Rect box, Axis axis, float delta, bool one_way) {
    const bool x = axis == Axis::X;
    const float lo = x ? box.x : box.y;
    const float len = x ? box.w : box.h;
    const CellRange range = cell_range(grid, box);
    if (range.empty()) return {lo + delta, false};
    const int count_across = x ? grid.height : grid.width;
    const Lines l{grid,
                  axis,
                  x ? grid.origin.x : grid.origin.y,
                  x ? grid.cell_w : grid.cell_h,
                  x ? grid.width : grid.height,
                  clamp_index(x ? range.y0 : range.x0, -1, count_across),
                  clamp_index(x ? range.y1 : range.x1, -1, count_across),
                  one_way,
                  lo + len};
    const float target = lo + delta;
    int hit = 0;
    if (delta > 0) {
        const int s = last_cell(l.origin, l.size, lo + len) + 1;
        if (!find_forward(l, s, last_cell(l.origin, l.size, target + len), hit)) return {target, false};
        const float pos = place_before(l, hit, len);
        return {pos < lo ? lo : (pos > target ? target : pos), true};
    }
    const int s = first_cell(l.origin, l.size, lo) - 1;
    if (!find_backward(l, s, first_cell(l.origin, l.size, target), hit)) return {target, false};
    const float pos = place_after(l, hit);
    return {pos > lo ? lo : (pos < target ? target : pos), true};
}

} // namespace detail

MoveResult move(const Grid& grid, Rect box, Vec2 delta, const MoveOptions& options) {
    NNH_ASSERT(std::isfinite(delta.x) && std::isfinite(delta.y), "collision::move: delta (%f, %f) not finite",
               static_cast<double>(delta.x), static_cast<double>(delta.y));
    NNH_ASSERT(grid.cell_w > 0 && grid.cell_h > 0, "collision::move: cell size must be > 0");
    if (!std::isfinite(delta.x)) delta.x = 0; // Release: a bad delta does not move
    if (!std::isfinite(delta.y)) delta.y = 0;
    MoveResult r;
    if (!(grid.cell_w > 0 && grid.cell_h > 0)) { // Release: a broken grid collides with nothing
        r.position = {box.x + delta.x, box.y + delta.y};
        r.moved = delta;
        return r;
    }
    const Vec2 start{box.x, box.y};
    if (delta.x != 0) {
        const detail::SweepResult s = detail::sweep(grid, box, detail::Axis::X, delta.x, false);
        box.x = s.position;
        r.moved.x = s.blocked ? box.x - start.x : delta.x;
        r.touching.right = s.blocked && delta.x > 0;
        r.touching.left = s.blocked && delta.x < 0;
    }
    if (delta.y != 0) {
        const bool one_way = delta.y > 0 && !options.drop_through;
        const detail::SweepResult s = detail::sweep(grid, box, detail::Axis::Y, delta.y, one_way);
        box.y = s.position;
        r.moved.y = s.blocked ? box.y - start.y : delta.y;
        r.touching.floor = s.blocked && delta.y > 0;
        r.touching.ceiling = s.blocked && delta.y < 0;
    }
    r.position = {box.x, box.y};
    return r;
}

} // namespace nnh::collision
