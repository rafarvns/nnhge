// Emission on demand (spec 024 FR-6, FR-9, FR-15, FR-18, plan "Births"): the birth map, the full case and emit /
// emit_at, through the shape's births (particle_births.cpp, also used by rate births, bursts and prewarm). The emission
// number is taken by every particle asked for (dropped ones too), so drops never shift later values. Full: as many as
// fit, the rest dropped and counted, one Debug/Development warning per emitter.

#include "nnh/log.hpp"
#include "particles/emitter_births.hpp"
#include "particles/particle_bits.hpp"

NNH_LOG_CHANNEL_EXTERN(particles);

namespace nnh::detail {

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
    b.derived.births(b, number, 1, at, pre_age);
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
    b.derived.births(b, first, made, m, 0.0f);
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
