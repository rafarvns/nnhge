#pragma once
// The slab test of one box (spec 019 FR-10, FR-13): a segment against an open box given per axis as an interval
// (lo, hi). Rays use the box itself; box casts the target grown by the cast box. Internal, pure.

#include "collision/exact.hpp"

namespace nnh::collision::detail {

/// The open interval (lo, hi) of a box on one axis.
struct Slab {
    double lo = 0;
    double hi = 0;
};

struct SlabHit {
    bool hit = false;    ///< the segment crosses the interior (touching is not hitting)
    bool inside = false; ///< the start is strictly inside (then `t` is 0 and no face)
    Ratio t{};           ///< where it enters (≥ 0)
    bool face_x = false; ///< entered through an x face (also on a corner tie); else a y face
};

/// Where the segment enters the open box x × y. An axis the segment doesn't move along needs lo < p < hi
/// strictly. Entering at t = 0 through a face is a hit with that face; a start strictly inside is `inside`.
SlabHit slab_cast(const Segment& s, Slab x, Slab y);

/// A box's slabs as the half-open rules see it: (x, x + w) with the sum in float, as overlaps() computes it.
inline Slab slab_x(Rect r) { return {r.x, static_cast<double>(r.x + r.w)}; }
inline Slab slab_y(Rect r) { return {r.y, static_cast<double>(r.y + r.h)}; }

} // namespace nnh::collision::detail
