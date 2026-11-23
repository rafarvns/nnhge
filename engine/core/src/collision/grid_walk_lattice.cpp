// The grid walk's lattice path (see grid_walk.hpp, lattice.hpp): the segment's ends, the grid's origin and
// cell size as integers of 1/256 px, so every line is origin + i · size exactly and the walk compares n_x ·
// a_y with n_y · a_x in int64, adding size · a to the crossed side per step. On these inputs the double
// path's differences and products are exact too (|values| < 2^24 units, products < 2^48), so both paths take
// the same decisions and queue the same Ratio values: t = n · 2^-8 / a, with the double path's own a.
//
// It starts at t0 = 0 only: an unclipped walk, or a clipped one starting strictly inside the grid (where the
// double path's clip keeps t0 = 0 as well); anything else takes the double path. Its segments cross at most
// a_x / size_x + a_y / size_y + 3 lines, within the double path's step bound, which it therefore doesn't
// count.

#include "collision/grid_walk.hpp"
#include "collision/lattice.hpp"

namespace nnh::collision::detail {

namespace {

/// floor(a / b) for b > 0 (one __aeabi_idivmod on the 3DS: once per axis per ray).
std::int32_t floor_div(std::int32_t a, std::int32_t b) {
    const std::int32_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

/// The start cell on one axis, as the double path's fixed_cell / moving_cell at t0 = 0 find it: the cell with
/// edge(k) ≤ p < edge(k + 1) when the axis goes + or doesn't move, edge(k) < p ≤ edge(k + 1) when it goes −.
int start_cell(const GridWalk::LatticeAxis& ax, int s) {
    const std::int32_t from = ax.p - ax.origin;
    return s >= 0 ? floor_div(from, ax.size) : -floor_div(-from, ax.size) - 1; // ceil(from / size) − 1
}

/// p strictly between the grid's first and last line (int64: the last may lie far beyond the lattice).
bool strictly_inside(const GridWalk::LatticeAxis& ax, int count) {
    const std::int64_t last = ax.origin + static_cast<std::int64_t>(count) * ax.size;
    return count > 0 && ax.origin < ax.p && ax.p < last;
}

bool axis_on_lattice(float from, float to, float origin, float size, GridWalk::LatticeAxis& ax) {
    std::int32_t end = 0;
    if (!to_lattice(from, ax.p) || !to_lattice(to, end) || !to_lattice(origin, ax.origin) ||
        !to_lattice(size, ax.size)) {
        return false;
    }
    ax.a = end > ax.p ? end - ax.p : ax.p - end;
    return ax.size > 0;
}

/// Distance from p to the line ahead of cell i (> 0 for the start cell and every later one).
std::int32_t ahead(const GridWalk::LatticeAxis& ax, int s, int i) {
    return s > 0 ? ax.edge(i + 1) - ax.p : ax.p - ax.edge(i);
}

} // namespace

bool GridWalk::lattice_inputs(const Grid& grid, const Segment& s) {
    if (!axis_on_lattice(s.from.x, s.to.x, grid.origin.x, grid.cell_w, lx_) ||
        !axis_on_lattice(s.from.y, s.to.y, grid.origin.y, grid.cell_h, ly_)) {
        return false;
    }
    return !clip_ || (strictly_inside(lx_, x_.count) && strictly_inside(ly_, y_.count));
}

bool GridWalk::start_lattice(const Segment& s) {
    i_ = start_cell(lx_, s.sx);
    j_ = start_cell(ly_, s.sy);
    if (s.sx == 0 && lx_.edge(i_) == lx_.p) return false;                   // along a vertical line
    if (s.sy == 0 && ly_.edge(j_) == ly_.p) return false;                   // along a horizontal one
    const bool fx = s.sx != 0 && lx_.edge(s.sx > 0 ? i_ : i_ + 1) == lx_.p; // on the start cell's back face
    const bool fy = s.sy != 0 && ly_.edge(s.sy > 0 ? j_ : j_ + 1) == ly_.p;
    push(i_, j_, kRatioZero, fx, fy, fx && fy);
    if (!fx && !fy && queued_ > 0) queue_[queued_ - 1].inside = true;
    lx_.n = ahead(lx_, s.sx, i_);
    ly_.n = ahead(ly_, s.sy, j_);
    lx_.cross = static_cast<std::int64_t>(lx_.n) * ly_.a;
    ly_.cross = static_cast<std::int64_t>(ly_.n) * lx_.a;
    lx_.cross_step = static_cast<std::int64_t>(lx_.size) * ly_.a;
    ly_.cross_step = static_cast<std::int64_t>(ly_.size) * lx_.a;
    return !clip_ || in_grid(i_, j_);
}

void GridWalk::advance_lattice() {
    const int order =
        x_.s == 0 ? 1 : (y_.s == 0 ? -1 : compare_i64(lx_.cross, ly_.cross)); // < 0: x line first
    const LatticeAxis& crossed = order <= 0 ? lx_ : ly_;
    if (crossed.n >= crossed.a) { // the line lies at t ≥ 1
        done_ = true;
        return;
    }
    const Ratio t{static_cast<double>(crossed.n) * kLatticeUnit, order <= 0 ? x_.a : y_.a};
    if (order < 0) {
        i_ += x_.s;
        push(i_, j_, t, true, false, false);
    } else if (order > 0) {
        j_ += y_.s;
        push(i_, j_, t, false, true, false);
    } else { // a corner: both cells beside it, then the diagonal one
        push(i_ + x_.s, j_, t, true, false, true);
        push(i_, j_ + y_.s, t, false, true, true);
        i_ += x_.s;
        j_ += y_.s;
        push(i_, j_, t, true, true, true);
    }
    if (order <= 0) { // the next line is one cell further: n grows by size on either direction
        lx_.n += lx_.size;
        lx_.cross += lx_.cross_step;
    }
    if (order >= 0) {
        ly_.n += ly_.size;
        ly_.cross += ly_.cross_step;
    }
    if (clip_ && !in_grid(i_, j_)) done_ = true;
}

} // namespace nnh::collision::detail
