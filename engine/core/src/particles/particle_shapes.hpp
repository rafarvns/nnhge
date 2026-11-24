#pragma once
// Shapes and directions (spec 024 FR-8, FR-9, plan decision 12): positions uniform over areas, edges and rings without a
// square root, outward directions from the sampled angle (circles) or a fixed Newton reciprocal square root (boxes,
// lines). Each shape's births are one function with its sampler and direction inline (particle_births.cpp), chosen once
// per emitter (T10.3).

#include <cstdint>

#include "nnh/particles.hpp"
#include "particles/particle_derived.hpp"

namespace nnh::detail {

/// The births of `shape` (`edge`: a box's outline).
BirthsFn births_for(ParticleShape shape, bool edge);

/// 1 / √x for a normal positive x: an integer seed and three Newton steps (× and − only): the same bits everywhere.
float reciprocal_sqrt(float x);

/// `v` scaled to unit length; false (out untouched) when |v|² is zero, subnormal or not finite.
bool unit_vector(Vec2 v, Vec2& out);

} // namespace nnh::detail
