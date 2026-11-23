#pragma once
// The caller's buffers of the "all" queries (spec 019 FR-11): capped, sorted by fraction, stable, with the
// total.

#include "nnh/collision.hpp"

namespace nnh::collision::detail {

/// Inserts `hit` into the `total` hits found so far (the first min(total, capacity) are in `hits`, sorted by
/// fraction): after any hit with the same or a smaller fraction, dropped when it falls past `capacity`.
/// Returns total + 1. `hits` may be null when capacity is 0.
int insert_hit(RayHit* hits, int capacity, int total, const RayHit& hit);

/// Appends `index` when there is room; returns total + 1.
inline int append_index(int* out, int capacity, int total, int index) {
    if (total < capacity) out[total] = index;
    return total + 1;
}

} // namespace nnh::collision::detail
