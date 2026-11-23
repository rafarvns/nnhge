// Capped sorted insertion of hits (spec 019 FR-11), see hit_list.hpp.

#include "collision/hit_list.hpp"

namespace nnh::collision::detail {

int insert_hit(RayHit* hits, int capacity, int total, const RayHit& hit) {
    const int kept = total < capacity ? total : capacity;
    int at = kept;
    while (at > 0 && hits[at - 1].fraction > hit.fraction)
        --at;
    if (at < capacity) {
        for (int i = (kept < capacity ? kept : capacity - 1); i > at; --i)
            hits[i] = hits[i - 1];
        hits[at] = hit;
    }
    return total + 1;
}

} // namespace nnh::collision::detail
