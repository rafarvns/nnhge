// Queries (spec 024 FR-22): a particle's view (appearance evaluated from its age), the counters, the capacity.

#include "particles/particle_appearance.hpp"

namespace nnh {

int ParticleEmitter::capacity() const { return block_ != nullptr ? block_->capacity : 0; }

ParticleView ParticleEmitter::particle(int index) const {
    const detail::ParticleBlock* b = block_;
    if (b == nullptr || index < 0 || index >= b->alive) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::particle: index outside 0..alive() - 1 (or an empty emitter)");
        return {};
    }
    const detail::ParticleHot& h = b->hot[index];
    const detail::ParticleCold& c = b->cold[index];
    const float t = detail::life_fraction(h, c);
    ParticleView v;
    v.position = {h.px, h.py};
    v.velocity = {h.vx, h.vy};
    v.rotation = h.rotation;
    v.size = detail::size_at(b->derived.size, t, c.size);
    v.color = detail::unpack_color(detail::color_at(b->derived.color, t, c.color));
    v.frame = detail::frame_at(b->derived, h, c, t);
    v.age = FrameTime::from_ticks(h.age);
    v.lifetime = FrameTime::from_ticks(h.life);
    return v;
}

ParticleStats ParticleEmitter::stats() const {
    const detail::ParticleBlock* b = block_;
    if (b == nullptr) return {};
    ParticleStats s;
    s.live = b->alive;
    s.peak = b->peak;
    s.capacity = b->capacity;
    s.emitted = b->emitted;
    s.dropped = b->dropped;
    s.drawn = b->drawn;
    s.culled = b->culled;
    return s;
}

} // namespace nnh
