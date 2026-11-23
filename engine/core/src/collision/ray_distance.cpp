// The length of a ray (spec 019 FR-16): the queries' only square root. IEEE 754 requires it correctly rounded, so
// every system agrees; compiled with -fno-math-errno (and -ffp-contract=off) so std::sqrt is one instruction
// (sqrtss on x86-64, vsqrt.f32 on the 3DS), never a call into the C library.

#include <cmath>

#include "collision/ray_hit.hpp"

namespace nnh::collision::detail {

float ray_length(Vec2 from, Vec2 to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace nnh::collision::detail
