// update() (spec 024 FR-11, FR-17, FR-18, plan "update()"): the clock, motion of the live particles (semi-implicit Euler
// per 60 Hz frame, in a fixed order), deaths with a stable compaction (emission order kept), bounds as order keys, rate
// births spread along the emitter's movement, bursts, then the previous transform. The motion loop is specialised per
// emitter (<spin, drag>): no option is tested per particle.

#include <cstdint>

#include "particles/emitter_clock.hpp"
#include "particles/particle_bits.hpp"
#include "particles/particle_map.hpp"

namespace nnh {
namespace detail {

void rate_births(ParticleBlock& b); // emitter_rate.cpp

namespace {

constexpr std::uint32_t kFrameTicks = FrameTime::kTicksPerFrame;

/// Gravity per frame for this update, in the particles' stored space.
Vec2 frame_gravity(const ParticleBlock& b) {
    const Vec2 g = b.derived.gravity;
    if (b.derived.world) {
        if (b.desc.world_gravity) return g;
        const Vec2 m = map_direction(b.now.map, g);
        return {flush_tiny(m.x), flush_tiny(m.y)};
    }
    if (!b.desc.world_gravity) return g;
    Affine2D inv;
    if (!inverse(b.now.map, inv)) return {};
    const Vec2 m = map_direction(inv, g);
    return {flush_tiny(m.x), flush_tiny(m.y)};
}

template <bool Spin, bool Drag>
void move_particles(ParticleBlock& b, Vec2 g) {
    ParticleHot* const hot = b.hot;
    ParticleCold* const cold = b.cold;
    const int n = b.alive;
    const float keep = b.derived.keep;
    std::int32_t min_x = INT32_MAX, min_y = INT32_MAX, max_x = INT32_MIN, max_y = INT32_MIN;
    int w = 0;
    for (int i = 0; i < n; ++i) {
#if defined(__GNUC__)
        __builtin_prefetch(reinterpret_cast<const char*>(hot) + (i + 4) * sizeof(ParticleHot));
#endif
        ParticleHot h = hot[i];
        h.age += kFrameTicks;
        if (h.age >= h.life) continue;
        float vx = h.vx + g.x;
        float vy = h.vy + g.y;
        if constexpr (Drag) {
            vx = flush_tiny(vx * keep);
            vy = flush_tiny(vy * keep);
        }
        h.vx = vx;
        h.vy = vy;
        h.px = h.px + vx;
        h.py = h.py + vy;
        if constexpr (Spin) h.rotation = h.rotation + h.spin;
        hot[w] = h;
        if (w != i) cold[w] = cold[i];
        const std::int32_t kx = order_key(h.px), ky = order_key(h.py);
        min_x = kx < min_x ? kx : min_x;
        max_x = kx > max_x ? kx : max_x;
        min_y = ky < min_y ? ky : min_y;
        max_y = ky > max_y ? ky : max_y;
        ++w;
    }
    b.alive = w;
    b.key_min_x = min_x;
    b.key_min_y = min_y;
    b.key_max_x = max_x;
    b.key_max_y = max_y;
}

using MoveFn = void (*)(ParticleBlock&, Vec2);
constexpr MoveFn kMoves[4] = {&move_particles<false, false>, &move_particles<false, true>,
                              &move_particles<true, false>, &move_particles<true, true>};

} // namespace

void emitter_step(ParticleBlock& b) {
    const Vec2 d = b.now.map.origin - b.before.map.origin;
    b.emitter_velocity = {flush_tiny(d.x), flush_tiny(d.y)};
    const ClockFrame f = clock_begin(b);
    const Vec2 g = frame_gravity(b);
    kMoves[(b.derived.spin ? 2 : 0) + (b.derived.drag ? 1 : 0)](b, g);
    if (f.rate) rate_births(b);
    clock_end(b, f);
    b.before = b.now;
}

void emitter_clear(ParticleBlock& b) { b.alive = 0; }

} // namespace detail

void ParticleEmitter::update() {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::update on an empty emitter");
        return;
    }
    detail::emitter_step(*block_);
}

void ParticleEmitter::clear() {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::clear on an empty emitter");
        return;
    }
    detail::emitter_clear(*block_);
}

} // namespace nnh
