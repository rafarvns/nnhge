#pragma once
// Shape samplers and directions (spec 024 FR-8, FR-9, plan decision 12): positions uniform over areas, edges and rings
// without a square root, outward directions from the sampled angle (circles) or a fixed Newton reciprocal square root
// (boxes, lines). One sampler per shape, chosen once per emitter.

#include <cstdint>

#include "nnh/particles.hpp"
#include "particles/particle_derived.hpp"

namespace nnh::detail {

/// The sampler of `shape` (`edge`: a box's outline).
ShapeSampler shape_sampler(ParticleShape shape, bool edge);

/// 1 / √x for a normal positive x: an integer seed and three Newton steps (× and − only): the same bits everywhere.
float reciprocal_sqrt(float x);

/// `v` scaled to unit length; false (out untouched) when |v|² is zero, subnormal or not finite.
bool unit_vector(Vec2 v, Vec2& out);

/// A particle's initial direction: the outward one when the description asks for it and the sample has one,
/// otherwise the cone's angle + (u − ½) × spread from property kPropDirection.
Vec2 particle_direction(const ParticleDerived& d, std::uint32_t base, const ShapeSample& sample);

} // namespace nnh::detail
