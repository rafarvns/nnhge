// Births (spec 024 FR-6, FR-9, FR-15, FR-18, plan "Births"): one inline birth() used by rate births, bursts, emit,
// emit_at and prewarm. Values in property order from the counter-based generator; the emission number is taken by every
// particle asked for (dropped ones too), so drops never shift later values. Full: as many as fit, the rest dropped and
// counted, one Debug/Development warning per emitter.

#include "nnh/log.hpp"
#include "particles/emitter_births.hpp"
#include "particles/particle_bits.hpp"
#include "particles/particle_random.hpp"
#include "particles/particle_shapes.hpp"

NNH_LOG_CHANNEL_EXTERN(particles);

namespace nnh::detail {
namespace {

std::uint32_t pack(const std::uint8_t c[4]) {
    return static_cast<std::uint32_t>(c[0]) | (static_cast<std::uint32_t>(c[1]) << 8) |
           (static_cast<std::uint32_t>(c[2]) << 16) | (static_cast<std::uint32_t>(c[3]) << 24);
}

void include_bounds(ParticleBlock& b, float x, float y) {
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

} // namespace

int reserve_births(ParticleBlock& b, int count) {
    const int room = b.capacity - b.alive;
    const int made = count < room ? count : room;
    if (made < count) {
        b.dropped += static_cast<std::uint32_t>(count - made);
#if !defined(NNH_FLAVOR_RELEASE)
        if (!b.warned_full) {
            NNH_LOG_WARN(particles, "emitter full (%d particles): new ones dropped", b.capacity);
        }
#endif
        b.warned_full = true;
    }
    return made;
}

void birth(ParticleBlock& b, std::uint32_t number, const BirthMap& at, float pre_age) {
    const ParticleDerived& d = b.derived;
    const std::uint32_t base = particle_base(b.seed, number);
    ShapeSample shape;
    d.sample(d, base, shape);
    const Vec2 dir = particle_direction(d, base, shape);
    const float speed = d.speed_min + d.speed_span * unit_float(particle_hash(base, kPropSpeed));
    Vec2 p = shape.position;
    Vec2 v{dir.x * speed, dir.y * speed};
    if (!at.translation) {
        p = map_point(at.map, p);
        v = map_direction(at.map, v);
    } else {
        p = {p.x + at.map.origin.x, p.y + at.map.origin.y};
    }
    v = {flush_tiny(v.x + at.inherit.x), flush_tiny(v.y + at.inherit.y)};
    const float rotation = d.rotation_min + d.rotation_span * unit_float(particle_hash(base, kPropRotation));
    const float spin = d.spin_min + d.spin_span * unit_float(particle_hash(base, kPropSpin));
    const std::uint32_t life = d.life_min + hash_below(particle_hash(base, kPropLifetime), d.life_count);
    const float cu = unit_float(particle_hash(base, kPropColor));
    std::uint8_t c[4];
    for (int k = 0; k < 4; ++k) {
        c[k] = static_cast<std::uint8_t>(static_cast<float>(d.color_min[k]) + d.color_span[k] * cu + 0.5f);
    }
    ParticleHot& h = b.hot[b.alive];
    h.px = p.x + v.x * pre_age;
    h.py = p.y + v.y * pre_age;
    h.vx = v.x;
    h.vy = v.y;
    h.rotation = at.rotation + flip_sign(rotation, at.mirror);
    h.spin = flip_sign(spin, at.mirror);
    h.age = static_cast<std::uint32_t>(pre_age * 4096.0f + 0.5f);
    h.life = life;
    ParticleCold& cold = b.cold[b.alive];
    cold.inv_life = 1.0f / static_cast<float>(life);
    cold.size = d.size_min + d.size_span * unit_float(particle_hash(base, kPropSize));
    cold.color = pack(c);
    cold.frame = d.random_frame ? static_cast<std::uint8_t>(hash_below(particle_hash(base, kPropFrame),
                                                                        static_cast<std::uint32_t>(d.frames)))
                                : 0;
    include_bounds(b, h.px, h.py);
    ++b.alive;
    ++b.emitted;
    if (b.alive > b.peak) b.peak = b.alive;
}

BirthMap birth_map(const ParticleBlock& b, const WorldTransform2D& at) {
    BirthMap m;
    m.map = at.map;
    m.translation = translation_only(at.map);
    m.rotation = at.rotation;
    m.mirror = mirror_bit(at.scale);
    if (b.derived.world) {
        m.inherit = {b.emitter_velocity.x * b.derived.inherit, b.emitter_velocity.y * b.derived.inherit};
    }
    return m;
}

int emit_on(ParticleBlock& b, const WorldTransform2D& at, int count) {
    const int made = reserve_births(b, count);
    const std::uint32_t first = b.next_number;
    b.next_number += static_cast<std::uint32_t>(count);
    const BirthMap m = birth_map(b, at);
    for (int i = 0; i < made; ++i) birth(b, first + static_cast<std::uint32_t>(i), m, 0.0f);
    return made;
}

} // namespace nnh::detail

namespace nnh {

int ParticleEmitter::emit(int count) {
    if (block_ == nullptr || count < 0) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::emit: an empty emitter or a negative count");
        return 0;
    }
    return detail::emit_on(*block_, detail::emitter_place(*block_), count);
}

int ParticleEmitter::emit_at(Vec2 point, int count) {
    if (block_ == nullptr || count < 0 || !detail::finite_float(point.x) || !detail::finite_float(point.y)) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::emit_at: an empty emitter, a negative count or a non-finite point");
        return 0;
    }
    WorldTransform2D at = detail::emitter_place(*block_);
    at.map.origin = point;
    return detail::emit_on(*block_, at, count);
}

int ParticleEmitter::emit_at(const WorldTransform2D& at, int count) {
    if (block_ == nullptr || count < 0 || !detail::finite_world(at)) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::emit_at: an empty emitter, a negative count or a non-finite transform");
        return 0;
    }
    return detail::emit_on(*block_, at, count);
}

} // namespace nnh
