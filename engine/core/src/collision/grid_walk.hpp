#pragma once
// The exact grid walk behind raycast, raycast_all and cells_crossed (spec 019 FR-9, FR-11): Amanatides & Woo's
// traversal, with every decision (which line comes first, corners, the start cell) made by exact ratio comparisons
// against the grid's own float edges (origin + i · size, as move() computes them). Internal, pure.
//
// Rules: a segment lying on a grid line visits nothing; the start cell is the one the segment goes into from
// `from` (entered through a face when `from` lies on its back face, x preferred, else `inside`); a step crosses
// the nearer line, and a corner (both lines at once) visits the x-neighbour, the y-neighbour, then the diagonal
// cell, all at that fraction; the walk stops before a line at t ≥ 1. With `clip`, only cells of the grid are
// visited (the segment is clipped to the grid's rectangle first).

#include "collision/exact.hpp"

namespace nnh::collision::detail {

/// A cell the walk visits.
struct WalkCell {
    int x = 0, y = 0;
    Ratio t{};             ///< where the segment enters it
    bool face_x = false;   ///< entered through an x face
    bool face_y = false;   ///< entered through a y face (both: through a corner)
    bool inside = false;   ///< `from` lies strictly inside it (no face)
    bool corner = false;   ///< entered exactly at a grid corner: `corner_point`
    Vec2 corner_point{};
};

class GridWalk {
public:
    GridWalk(const Grid& grid, const Segment& s, bool clip);

    /// The next cell, in order; false when the walk is over.
    bool next(WalkCell& cell);

    float edge_x(int line) const { return x_.edge(line); }
    float edge_y(int line) const { return y_.edge(line); }

    /// One axis of the grid seen by the segment.
    struct Axis {
        double p = 0, a = 0;
        int s = 0;
        float origin = 0, size = 1;
        int count = 0;
        float edge(int line) const { return origin + static_cast<float>(line) * size; }
        Ratio line_t(int line) const {
            const double n = static_cast<double>(edge(line)) - p;
            return {s > 0 ? n : -n, a};
        }
        Ratio back(int i) const { return line_t(s > 0 ? i : i + 1); }  ///< the line behind cell i
        Ratio front(int i) const { return line_t(s > 0 ? i + 1 : i); } ///< the line ahead of cell i
    };

private:
    /// Queues the first cells; false when nothing follows them.
    bool start(const Segment& s);
    bool clip_to_grid(Ratio& t0) const;
    void advance();
    void push(int x, int y, Ratio t, bool face_x, bool face_y, bool corner);
    bool in_grid(int x, int y) const { return x >= 0 && y >= 0 && x < x_.count && y < y_.count; }

    Axis x_, y_;
    bool clip_ = false;
    bool done_ = false;
    int i_ = 0, j_ = 0;
    long long steps_left_ = 0;
    WalkCell queue_[3];
    int queued_ = 0, read_ = 0;
};

} // namespace nnh::collision::detail
