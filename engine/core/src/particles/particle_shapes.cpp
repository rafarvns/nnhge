// The outward direction of boxes and lines (spec 024 FR-9): a unit vector by a fixed Newton reciprocal square root, the
// same bits everywhere. The samplers themselves live inline in each shape's births (particle_births.cpp).

#include "particles/particle_shapes.hpp"

#include <bit>

namespace nnh::detail {

float reciprocal_sqrt(float x) {
    const std::uint32_t seed = 0x5F3759DFu - (std::bit_cast<std::uint32_t>(x) >> 1);
    float y = std::bit_cast<float>(seed);
    const float half = 0.5f * x;
    for (int i = 0; i < 3; ++i) y = y * (1.5f - half * y * y);
    return y;
}

bool unit_vector(Vec2 v, Vec2& out) {
    const float len2 = v.x * v.x + v.y * v.y;
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(len2);
    if (bits < 0x00800000u || bits >= 0x7F800000u) return false; // zero, subnormal, infinite or NaN
    const float inv = reciprocal_sqrt(len2);
    out = {v.x * inv, v.y * inv};
    return true;
}

} // namespace nnh::detail
