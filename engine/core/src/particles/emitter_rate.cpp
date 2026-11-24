// Rate births (spec 024 FR-4, FR-10, plan "Rate births"): the accumulator makes rate_whole particles per frame plus one
// at each crossing of the threshold, exact over time. In world space particle j is born at its frame fraction
// f = (threshold·(j+1) − a₀)/step on the transform interpolated between the previous and the current one (one sine only
// when the rotation or scale changed; f = 1 is the current transform exactly), then pre-aged by 1 − f. Local space: born
// at the frame's end, age 0 (the frame moves with the emitter).

#include <bit>
#include <cstdint>

#include "particles/emitter_births.hpp"
#include "transform/compose.hpp"

namespace nnh::detail {
namespace {

/// The emitter's transform at fraction `f` of the last frame (0 < f < 1).
WorldTransform2D interpolated(const WorldTransform2D& a, const WorldTransform2D& b, float f) {
    WorldTransform2D t;
    const Vec2 o{a.map.origin.x + (b.map.origin.x - a.map.origin.x) * f,
                 a.map.origin.y + (b.map.origin.y - a.map.origin.y) * f};
    const bool same_axes = float_bits(a.rotation) == float_bits(b.rotation) &&
                           float_bits(a.scale.x) == float_bits(b.scale.x) &&
                           float_bits(a.scale.y) == float_bits(b.scale.y);
    if (same_axes) {
        t = b;
        t.map.origin = o;
        return t;
    }
    const Transform2D local{o, a.rotation + (b.rotation - a.rotation) * f,
                            {a.scale.x + (b.scale.x - a.scale.x) * f, a.scale.y + (b.scale.y - a.scale.y) * f}};
    root_world(local, sin_cos_inline(local.rotation), t);
    return t;
}

} // namespace

void rate_births(ParticleBlock& b) {
    const ParticleDerived& d = b.derived;
    const std::uint32_t a0 = b.accumulator;
    std::uint32_t count = d.rate_whole;
    std::uint32_t acc = a0 + d.rate_frac;
    if (acc >= kRateThreshold) {
        acc -= kRateThreshold;
        ++count;
    }
    b.accumulator = acc;
    if (count == 0) return;
    const int made = reserve_births(b, static_cast<int>(count));
    const std::uint32_t first = b.next_number;
    b.next_number += count;
    if (!d.world) {
        const BirthMap m = birth_map(b, emitter_place(b));
        for (int j = 0; j < made; ++j) birth(b, first + static_cast<std::uint32_t>(j), m, 0.0f);
        return;
    }
    const float f0 = static_cast<float>(kRateThreshold - a0) * d.rate_first;
    for (int j = 0; j < made; ++j) {
        float f = f0 + static_cast<float>(j) * d.rate_spacing;
        if (std::bit_cast<std::uint32_t>(f) >= kOneBits) f = 1.0f; // f > 0: the bits order as the values
        const bool end = std::bit_cast<std::uint32_t>(f) == kOneBits;
        const BirthMap m = birth_map(b, end ? b.now : interpolated(b.before, b.now, f));
        birth(b, first + static_cast<std::uint32_t>(j), m, 1.0f - f);
    }
}

} // namespace nnh::detail
