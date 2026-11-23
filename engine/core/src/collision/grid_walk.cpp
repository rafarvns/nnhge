// The exact grid walk (spec 019 FR-9), see grid_walk.hpp.

#include "collision/grid_walk.hpp"

#include <initializer_list>

#include "collision/grid_internal.hpp"

namespace nnh::collision::detail {

namespace {

constexpr int kMaxFix = 8;                   // corrections of a float guess (one or two are ever needed)
constexpr double kMaxSteps = 67108864.0;     // 2^26 lines: far beyond any real ray, keeps the count bounded

/// The cell holding `v` on an axis the segment doesn't move along, by the grid's own edges.
int fixed_cell(const GridWalk::Axis& ax) {
    int k = first_cell(ax.origin, ax.size, static_cast<float>(ax.p));
    for (int n = 0; n < kMaxFix && ax.p < static_cast<double>(ax.edge(k)); ++n) --k;
    for (int n = 0; n < kMaxFix && ax.p >= static_cast<double>(ax.edge(k + 1)); ++n) ++k;
    return k;
}

/// The cell the segment is in just after `t0` on a moving axis: back(k) ≤ t0 < front(k).
int moving_cell(const GridWalk::Axis& ax, Ratio t0) {
    const double d = ax.s > 0 ? ax.a : -ax.a;
    int k = first_cell(ax.origin, ax.size, along(ax.p, d, t0));
    for (int n = 0; n < kMaxFix && compare(ax.back(k), t0) > 0; ++n) k -= ax.s;
    for (int n = 0; n < kMaxFix && compare(t0, ax.front(k)) >= 0; ++n) k += ax.s;
    return k;
}

double cells_along(const GridWalk::Axis& ax) {
    const double size = ax.size;
    return ax.a / size;
}

} // namespace

GridWalk::GridWalk(const Grid& grid, const Segment& s, bool clip) : clip_(clip) {
    x_ = {s.px, s.ax, s.sx, grid.origin.x, grid.cell_w, grid.width};
    y_ = {s.py, s.ay, s.sy, grid.origin.y, grid.cell_h, grid.height};
    done_ = !start(s);
}

bool GridWalk::start(const Segment& s) {
    if (zero_length(s)) return false;
    if (s.sx == 0 && static_cast<double>(x_.edge(fixed_cell(x_))) == x_.p) return false; // along a vertical line
    if (s.sy == 0 && static_cast<double>(y_.edge(fixed_cell(y_))) == y_.p) return false; // along a horizontal one
    Ratio t0 = kRatioZero;
    if (clip_ && !clip_to_grid(t0)) return false;
    i_ = s.sx == 0 ? fixed_cell(x_) : moving_cell(x_, t0);
    j_ = s.sy == 0 ? fixed_cell(y_) : moving_cell(y_, t0);
    const double steps = 4 + cells_along(x_) + cells_along(y_);
    steps_left_ = static_cast<long long>(steps < kMaxSteps ? steps : kMaxSteps);
    const bool fx = s.sx != 0 && compare(x_.back(i_), t0) == 0;
    const bool fy = s.sy != 0 && compare(y_.back(j_), t0) == 0;
    if (fx && fy && t0.n > 0) { // clipped exactly at a grid corner: the cells beside it first
        push(i_, j_ - s.sy, t0, true, false, true);
        push(i_ - s.sx, j_, t0, false, true, true);
    }
    push(i_, j_, t0, fx, fy, fx && fy);
    if (!fx && !fy && queued_ > 0) queue_[queued_ - 1].inside = true;
    return !clip_ || in_grid(i_, j_); // clipped at a corner of the grid only: nothing after the cells beside it
}

bool GridWalk::clip_to_grid(Ratio& t0) const {
    Ratio enter = kRatioZero;
    Ratio exit = kRatioOne;
    for (const Axis* ax : {&x_, &y_}) {
        if (ax->count <= 0) return false;
        if (ax->s == 0) {
            if (!(static_cast<double>(ax->edge(0)) < ax->p && ax->p < static_cast<double>(ax->edge(ax->count)))) {
                return false;
            }
            continue;
        }
        const Ratio in = ax->line_t(ax->s > 0 ? 0 : ax->count);
        const Ratio out = ax->line_t(ax->s > 0 ? ax->count : 0);
        if (compare(in, enter) > 0) enter = in;
        if (compare(out, exit) < 0) exit = out;
    }
    // enter == exit: the segment only touches the grid's rectangle at one of its corners, which may still pass a
    // corner cell of the grid beside it (the corner rule); the walk starts outside and keeps only that cell.
    if (compare(enter, exit) > 0 || compare(enter, kRatioOne) >= 0) return false;
    t0 = enter;
    return true;
}

void GridWalk::push(int x, int y, Ratio t, bool face_x, bool face_y, bool corner) {
    if (clip_ && !in_grid(x, y)) return;
    WalkCell& c = queue_[queued_++];
    c = {};
    c.x = x;
    c.y = y;
    c.t = t;
    c.face_x = face_x;
    c.face_y = face_y;
    c.corner = corner;
    if (corner) {
        // The corner both lines meet at: the x line between this cell's column and the walk's, the same for y.
        const int cx = face_x ? (x_.s > 0 ? x : x + 1) : (x_.s > 0 ? x + 1 : x);
        const int cy = face_y ? (y_.s > 0 ? y : y + 1) : (y_.s > 0 ? y + 1 : y);
        c.corner_point = {x_.edge(cx), y_.edge(cy)};
    }
}

void GridWalk::advance() {
    if (steps_left_-- <= 0) {
        done_ = true;
        return;
    }
    const Ratio tx = x_.s != 0 ? x_.front(i_) : kRatioOne;
    const Ratio ty = y_.s != 0 ? y_.front(j_) : kRatioOne;
    const int order = x_.s == 0 ? 1 : (y_.s == 0 ? -1 : compare(tx, ty)); // < 0: the x line first
    const Ratio t = order <= 0 ? tx : ty;
    if (compare(t, kRatioOne) >= 0) {
        done_ = true;
        return;
    }
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
    const bool saturated = i_ <= -kMaxCellIndex || i_ >= kMaxCellIndex || j_ <= -kMaxCellIndex ||
                           j_ >= kMaxCellIndex;
    if ((clip_ && !in_grid(i_, j_)) || saturated) done_ = true;
}

bool GridWalk::next(WalkCell& cell) {
    while (read_ == queued_) {
        if (done_) return false;
        read_ = queued_ = 0;
        advance();
    }
    cell = queue_[read_++];
    return true;
}

} // namespace nnh::collision::detail
