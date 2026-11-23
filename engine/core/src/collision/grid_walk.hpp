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
//
// Two paths, the same cells bit for bit (plan "Console bench refinement (round 2)"): when the segment's ends, the
// grid's origin and cell size are on the 1/256 px lattice (lattice.hpp) and a clipped walk starts strictly inside
// the grid, the walk steps in integers (grid_walk_lattice.cpp: an int64 compare per step, no VFP compare on the
// 3DS); otherwise in double (grid_walk.cpp). Both produce the same Ratio values, since on the lattice every
// difference and product of the double path is exact.

#include <cstdint>

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

/// Which path a walk may take (tests compare the two).
enum class WalkPath : unsigned char {
    Any,    ///< integers when the inputs are on the lattice, else double
    Double, ///< always double
};

class GridWalk {
public:
    GridWalk(const Grid& grid, const Segment& s, bool clip, WalkPath path = WalkPath::Any);

    /// The next cell, in order, or null when the walk is over; valid until the next call. Inline: called per cell.
    const WalkCell* step() {
        while (read_ == queued_) {
            if (done_) return nullptr;
            read_ = queued_ = 0;
            if (lattice_) {
                advance_lattice();
            } else {
                advance();
            }
        }
        return &queue_[read_++];
    }

    /// step(), copied into `cell`; false when the walk is over.
    bool next(WalkCell& cell) {
        const WalkCell* c = step();
        if (c == nullptr) return false;
        cell = *c;
        return true;
    }

    /// The walk steps in integers (the lattice path).
    bool uses_lattice() const { return lattice_; }

    /// Lines the double path may still cross before it gives up (tests: a clipped walk stops when it leaves the
    /// grid). The lattice path never needs the bound (its segments cross fewer lines) and doesn't count.
    long long steps_left() const { return steps_left_; }

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

    /// One axis on the lattice (1/256 px units).
    struct LatticeAxis {
        std::int32_t p = 0, a = 0;         ///< start, |d|
        std::int32_t origin = 0, size = 1; ///< the grid's
        std::int32_t n = 0;                ///< distance to the next line ahead: t = n / a
        std::int64_t cross = 0;            ///< n · the other axis' a: the per-step compare
        std::int64_t cross_step = 0;       ///< size · the other axis' a: what a crossed line adds to `cross`
        std::int32_t edge(int line) const { return origin + line * size; }
    };

private:
    /// Queues the first cells; false when nothing follows them.
    bool start(const Segment& s);
    bool clip_to_grid(Ratio& t0) const;
    void advance();
    /// The lattice path (grid_walk_lattice.cpp): its inputs, its start (t0 = 0), its step.
    bool lattice_inputs(const Grid& grid, const Segment& s);
    bool start_lattice(const Segment& s);
    void advance_lattice();
    void push(int x, int y, Ratio t, bool face_x, bool face_y, bool corner);
    bool in_grid(int x, int y) const { return x >= 0 && y >= 0 && x < x_.count && y < y_.count; }

    Axis x_, y_;
    LatticeAxis lx_, ly_;
    bool clip_ = false;
    bool done_ = false;
    bool lattice_ = false;
    int i_ = 0, j_ = 0;
    Ratio tx_{}, ty_{}; ///< where the segment meets the next x / y line (kept per step: only the crossed one changes)
    long long steps_left_ = 0;
    WalkCell queue_[3];
    int queued_ = 0, read_ = 0;
};

// Inline: both paths push per step.
inline void GridWalk::push(int x, int y, Ratio t, bool face_x, bool face_y, bool corner) {
    if (clip_ && !in_grid(x, y)) return;
    WalkCell& c = queue_[queued_++];
    c.x = x; // every field set (no `c = {}`: a memset call per cell on the 3DS)
    c.y = y;
    c.t = t;
    c.face_x = face_x;
    c.face_y = face_y;
    c.inside = false;
    c.corner = corner;
    c.corner_point = {};
    if (corner) {
        // The corner both lines meet at: the x line between this cell's column and the walk's, the same for y.
        const int cx = face_x ? (x_.s > 0 ? x : x + 1) : (x_.s > 0 ? x + 1 : x);
        const int cy = face_y ? (y_.s > 0 ? y : y + 1) : (y_.s > 0 ? y + 1 : y);
        c.corner_point = {x_.edge(cx), y_.edge(cy)};
    }
}

} // namespace nnh::collision::detail
