// Births per shape (spec 024 FR-6, FR-8, FR-9, FR-15; T10.3): one function per shape with its sampler, its
// direction and the rest of the birth inline, chosen once per emitter (ParticleDerived::births). Values come
// in property order from the counter-based generator exactly as before T10.3
// (tests/particles/test_birth_golden.cpp holds their bits); what changed is the cost on the ARM11: no
// indirect call per particle, the circle's two sines in one block (sin_cos_flat2: their chains interleave),
// integer compares instead of float ones (no vmrs), the colour's four channels side by side, and the next
// records fetched ahead (the 3DS has no hardware prefetcher).

#include "particles/emitter_births.hpp"
#include "particles/particle_bits.hpp"
#include "particles/particle_random.hpp"
#include "particles/particle_shapes.hpp"
#include "transform/sin_cos_flat.hpp"

namespace nnh::detail {
namespace {

enum class Kind { Point, BoxArea, BoxEdge, Circle, Line };

NNH_DET_INLINE float unit(std::uint32_t base, std::uint32_t property) {
    return unit_float(particle_hash(base, property));
}

/// The cone's direction: cone_start + u × spread.
NNH_DET_INLINE Vec2 cone_direction(const ParticleDerived& d, std::uint32_t base) {
    const SinCos sc = sin_cos_flat(d.cone_start + unit(base, kPropDirection) * d.cone_spread);
    return {sc.cos, sc.sin};
}

/// Boxes and lines: outward from the centre when asked for and defined, else the cone.
NNH_DET_INLINE Vec2 outward_or_cone(const ParticleDerived& d, std::uint32_t base, Vec2 p) {
    Vec2 out;
    if (!d.cone && unit_vector(p, out)) return out;
    return cone_direction(d, base);
}

/// Uniform along a box's outline by length: top (left to right), right (down), bottom (right to left), left
/// (up). The lengths are non-negative floats, whose bits order as their values.
NNH_DET_INLINE Vec2 box_edge(const ParticleDerived& d, std::uint32_t base) {
    const float s = unit(base, kPropShapeA) * d.perimeter;
    const std::uint32_t k = float_bits(s);
    const Vec2 h = d.half_box;
    if (k < float_bits(d.edge_top)) return {s - h.x, 0.0f - h.y};
    if (k < float_bits(d.edge_right)) return {h.x, (s - d.edge_top) - h.y};
    if (k < float_bits(d.edge_bottom)) return {h.x - (s - d.edge_right), h.y};
    return {0.0f - h.x, h.y - (s - d.edge_bottom)};
}

/// A shape-local position and a unit direction.
template <Kind K>
NNH_DET_INLINE void place(const ParticleDerived& d, std::uint32_t base, Vec2& p, Vec2& dir) {
    if constexpr (K == Kind::Point) {
        p = {};
        dir = cone_direction(d, base);
    } else if constexpr (K == Kind::BoxArea) {
        p = {(unit(base, kPropShapeA) - 0.5f) * d.box.x, (unit(base, kPropShapeB) - 0.5f) * d.box.y};
        dir = outward_or_cone(d, base, p);
    } else if constexpr (K == Kind::BoxEdge) {
        p = box_edge(d, base);
        dir = outward_or_cone(d, base, p);
    } else if constexpr (K == Kind::Line) {
        p = {(unit(base, kPropShapeA) - 0.5f) * d.length, 0.0f};
        dir = outward_or_cone(d, base, p);
    } else {
        // Area-uniform radius fraction: the pick's uniform one, else the larger of two (compared as 24-bit
        // integers, which order as their unit floats).
        const float angle = d.arc_start + unit(base, kPropShapeA) * d.arc;
        const std::uint32_t m1 = particle_hash(base, kPropShapeB) >> 8;
        const std::uint32_t m2 = particle_hash(base, kPropShapeC) >> 8;
        const std::uint32_t pick = particle_hash(base, kPropShapeD) >> 8;
        const std::uint32_t m = pick < d.ring_pick ? m1 : (m1 > m2 ? m1 : m2);
        const float r = d.inner + d.ring_span * (static_cast<float>(m) * (1.0f / 16777216.0f));
        if (d.cone) {
            SinCos sc, cc;
            sin_cos_flat2(angle, d.cone_start + unit(base, kPropDirection) * d.cone_spread, sc, cc);
            p = {sc.cos * r, sc.sin * r};
            dir = {cc.cos, cc.sin};
        } else {
            const SinCos sc = sin_cos_flat(angle);
            p = {sc.cos * r, sc.sin * r};
            dir = {sc.cos, sc.sin};
        }
    }
}

NNH_DET_INLINE void include_bounds(ParticleBlock& b, float x, float y) {
    const std::int32_t kx = order_key(x), ky = order_key(y);
    if (b.alive == 0) {
        b.key_min_x = b.key_max_x = kx;
        b.key_min_y = b.key_max_y = ky;
        return;
    }
    if (kx < b.key_min_x) b.key_min_x = kx;
    if (kx > b.key_max_x) b.key_max_x = kx;
    if (ky < b.key_min_y) b.key_min_y = ky;
    if (ky > b.key_max_y) b.key_max_y = ky;
}

/// One channel of the start colour: base + span × u, rounded.
NNH_DET_INLINE std::uint32_t channel(const ParticleDerived& d, int k, float u) {
    return static_cast<std::uint8_t>(d.color_base[k] + d.color_span[k] * u + 0.5f);
}

template <Kind K>
NNH_DET_INLINE void birth_one(ParticleBlock& b, std::uint32_t number, const BirthMap& at, float pre_age) {
    const ParticleDerived& d = b.derived;
    const std::uint32_t base = particle_base(b.seed, number);
    Vec2 p, dir;
    place<K>(d, base, p, dir);
    const float speed = d.speed_min + d.speed_span * unit(base, kPropSpeed);
    Vec2 v{dir.x * speed, dir.y * speed};
    if (!at.translation) {
        p = map_point(at.map, p);
        v = map_direction(at.map, v);
    } else {
        p = {p.x + at.map.origin.x, p.y + at.map.origin.y};
    }
    v = {flush_tiny(v.x + at.inherit.x), flush_tiny(v.y + at.inherit.y)};
    const float rotation = d.rotation_min + d.rotation_span * unit(base, kPropRotation);
    const float spin = d.spin_min + d.spin_span * unit(base, kPropSpin);
    const std::uint32_t life = d.life_min + hash_below(particle_hash(base, kPropLifetime), d.life_count);
    const float cu = unit(base, kPropColor);
    const std::uint32_t color =
        channel(d, 0, cu) | (channel(d, 1, cu) << 8) | (channel(d, 2, cu) << 16) | (channel(d, 3, cu) << 24);
    const float size = d.size_min + d.size_span * unit(base, kPropSize);
    const std::uint32_t frame =
        d.random_frame ? hash_below(particle_hash(base, kPropFrame), static_cast<std::uint32_t>(d.frames))
                       : 0u;
    const int i = b.alive;
    ParticleHot& h = b.hot[i];
    h.px = p.x + v.x * pre_age;
    h.py = p.y + v.y * pre_age;
    h.vx = v.x;
    h.vy = v.y;
    h.rotation = at.rotation + flip_sign(rotation, at.mirror);
    h.spin = flip_sign(spin, at.mirror);
    h.age = static_cast<std::uint32_t>(pre_age * 4096.0f + 0.5f);
    h.life = life;
    ParticleCold& c = b.cold[i];
    c.inv_life = 1.0f / static_cast<float>(life);
    c.size = size;
    c.color = color;
    c.frame = static_cast<std::uint8_t>(frame);
    include_bounds(b, h.px, h.py);
    b.alive = i + 1;
    ++b.emitted;
    if (b.alive > b.peak) b.peak = b.alive;
}

template <Kind K>
void births(ParticleBlock& b, std::uint32_t first, int count, const BirthMap& at, float pre_age) {
    for (int n = 0; n < count; ++n) {
#if defined(__GNUC__)
        // Fresh records: fetch the ones four births ahead (no hardware prefetcher on the 3DS).
        __builtin_prefetch(b.hot + b.alive + 4, 1);
        __builtin_prefetch(b.cold + b.alive + 4, 1);
#endif
        birth_one<K>(b, first + static_cast<std::uint32_t>(n), at, pre_age);
    }
}

} // namespace

BirthsFn births_for(ParticleShape shape, bool edge) {
    switch (shape) {
    case ParticleShape::Box:
        return edge ? &births<Kind::BoxEdge> : &births<Kind::BoxArea>;
    case ParticleShape::Circle:
        return &births<Kind::Circle>;
    case ParticleShape::Line:
        return &births<Kind::Line>;
    default:
        return &births<Kind::Point>;
    }
}

} // namespace nnh::detail
