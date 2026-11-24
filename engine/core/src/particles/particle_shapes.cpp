// Shape samplers and directions (spec 024 FR-8, FR-9). Area-uniform discs and rings need no square root: a disc's
// radius fraction is max(u₁, u₂) (density 2x), a ring's a fixed mixture of a uniform (weight 2a/(a+b)) and that maximum,
// which is exactly the density of r ∝ r on [a, b].

#include "particles/particle_shapes.hpp"

#include <bit>

#include "particles/particle_random.hpp"
#include "transform/sin_cos.hpp"

namespace nnh::detail {
namespace {

float value(std::uint32_t base, std::uint32_t property) { return unit_float(particle_hash(base, property)); }

void sample_point(const ParticleDerived&, std::uint32_t, ShapeSample& out) { out = ShapeSample{}; }

void sample_box_area(const ParticleDerived& d, std::uint32_t base, ShapeSample& out) {
    const float ux = value(base, kPropShapeA);
    const float uy = value(base, kPropShapeB);
    out.position = {(ux - 0.5f) * d.box.x, (uy - 0.5f) * d.box.y};
    out.has_outward = unit_vector(out.position, out.outward);
}

/// Uniform along the outline by length: top (left to right), right (down), bottom (right to left), left (up).
void sample_box_edge(const ParticleDerived& d, std::uint32_t base, ShapeSample& out) {
    const float s = value(base, kPropShapeA) * d.perimeter;
    const Vec2 h = d.half_box;
    if (s < d.edge_top) {
        out.position = {s - h.x, 0.0f - h.y};
    } else if (s < d.edge_right) {
        out.position = {h.x, (s - d.edge_top) - h.y};
    } else if (s < d.edge_bottom) {
        out.position = {h.x - (s - d.edge_right), h.y};
    } else {
        out.position = {0.0f - h.x, h.y - (s - d.edge_bottom)};
    }
    out.has_outward = unit_vector(out.position, out.outward);
}

void sample_circle(const ParticleDerived& d, std::uint32_t base, ShapeSample& out) {
    const float angle = d.arc_start + value(base, kPropShapeA) * d.arc;
    const float x1 = value(base, kPropShapeB);
    const float x2 = value(base, kPropShapeC);
    const float pick = value(base, kPropShapeD);
    const float x = pick < d.ring_uniform ? x1 : (x1 > x2 ? x1 : x2);
    const float r = d.inner + d.ring_span * x;
    const SinCos sc = sin_cos_inline(angle);
    out.position = {sc.cos * r, sc.sin * r};
    out.outward = {sc.cos, sc.sin};
    out.has_outward = true;
}

void sample_line(const ParticleDerived& d, std::uint32_t base, ShapeSample& out) {
    out.position = {(value(base, kPropShapeA) - 0.5f) * d.length, 0.0f};
    out.has_outward = unit_vector(out.position, out.outward);
}

} // namespace

ShapeSampler shape_sampler(ParticleShape shape, bool edge) {
    switch (shape) {
    case ParticleShape::Box:
        return edge ? &sample_box_edge : &sample_box_area;
    case ParticleShape::Circle:
        return &sample_circle;
    case ParticleShape::Line:
        return &sample_line;
    default:
        return &sample_point;
    }
}

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

Vec2 particle_direction(const ParticleDerived& d, std::uint32_t base, const ShapeSample& sample) {
    if (!d.cone && sample.has_outward) return sample.outward;
    const float angle = d.cone_start + value(base, kPropDirection) * d.cone_spread;
    const SinCos sc = sin_cos_inline(angle);
    return {sc.cos, sc.sin};
}

} // namespace nnh::detail
