// The slab test (spec 019 FR-10), see ray_box.hpp. Entry and exit parameters per axis are ratios with the axis'
// |d| as denominator; every decision compares them exactly.

#include "collision/ray_box.hpp"

namespace nnh::collision::detail {

namespace {

struct Interval {
    bool moving = false; ///< false: the segment doesn't move on this axis (the interval is everything)
    Ratio in{}, out{};
};

/// The segment's parameters inside (lo, hi) on one axis; false when it can't be inside at all.
bool axis_interval(double p, double a, int s, Slab slab, Interval& out) {
    if (s == 0) return slab.lo < p && p < slab.hi;
    out.moving = true;
    out.in = s > 0 ? Ratio{slab.lo - p, a} : Ratio{p - slab.hi, a};
    out.out = s > 0 ? Ratio{slab.hi - p, a} : Ratio{p - slab.lo, a};
    return true;
}

} // namespace

SlabHit slab_cast(const Segment& s, Slab x, Slab y) {
    SlabHit r;
    Interval ix, iy;
    if (zero_length(s) || !axis_interval(s.px, s.ax, s.sx, x, ix) || !axis_interval(s.py, s.ay, s.sy, y, iy)) {
        return r;
    }
    // Entry: the later of the two entries (x on a tie); exit: the earlier exit.
    const bool x_enters = ix.moving && (!iy.moving || compare(ix.in, iy.in) >= 0);
    const Ratio enter = x_enters ? ix.in : iy.in;
    const Ratio exit = !ix.moving ? iy.out : (!iy.moving || compare(ix.out, iy.out) <= 0 ? ix.out : iy.out);
    if (!(compare(enter, exit) < 0 && before_end(enter) && exit.n > 0)) return r;
    r.hit = true;
    if (enter.n < 0) {
        r.inside = true;
        r.t = kRatioZero;
        return r;
    }
    r.t = enter;
    r.face_x = x_enters;
    return r;
}

} // namespace nnh::collision::detail
