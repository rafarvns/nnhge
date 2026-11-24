#pragma once
// The smoothing walk (spec 025 FR-15, plan decision 18): spec 019's exact grid traversal, copied over integers for a
// segment between two cell centres. In doubled coordinates centres are odd and grid lines even, so the segment never
// lies on a line; the next x line is at t = (2kx + 1) / (2|dx|) and the next y line at (2ky + 1) / (2|dy|) after kx and
// ky lines, compared as (2kx + 1)·|dy| against (2ky + 1)·|dx| (32 bits: <= 2047 x 1023). At an exact corner the walk
// enters the diagonal cell and reports a corner: the two cells beside it, (x, y - sy) then (x - sx, y), were only
// touched at the point (spec 019 lists them first). Equal to collision::cells_crossed on a grid, cell for cell (tested).

#include "nnh/pathfinding.hpp"

namespace nnh::detail {

/// A cell the walk enters, after the start cell.
struct WalkStep {
    int x = 0;
    int y = 0;
    bool corner = false; ///< entered through a grid corner: side cells (x, y - sy) and (x - sx, y)
    int sx = 0;          ///< the walk's x direction (-1, 0, 1)
    int sy = 0;
};

class SmoothWalk {
public:
    SmoothWalk(CellPos a, CellPos b) : x_(a.x), y_(a.y) {
        const int dx = b.x - a.x;
        const int dy = b.y - a.y;
        sx_ = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
        sy_ = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
        ax_ = dx < 0 ? -dx : dx;
        ay_ = dy < 0 ? -dy : dy;
    }

    /// The next cell; false at the end (after b).
    bool next(WalkStep& s) {
        if (kx_ == ax_ && ky_ == ay_) return false;
        const int lhs = (2 * kx_ + 1) * ay_; // x line first when smaller (also when ay_ = 0, or y done)
        const int rhs = (2 * ky_ + 1) * ax_;
        s.corner = lhs == rhs;
        if (lhs <= rhs) {
            x_ += sx_;
            ++kx_;
        }
        if (lhs >= rhs) {
            y_ += sy_;
            ++ky_;
        }
        s.x = x_;
        s.y = y_;
        s.sx = sx_;
        s.sy = sy_;
        return true;
    }

private:
    int x_, y_;
    int sx_ = 0, sy_ = 0;
    int ax_ = 0, ay_ = 0;
    int kx_ = 0, ky_ = 0;
};

} // namespace nnh::detail
