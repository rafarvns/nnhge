// The emitter's clock (spec 024 FR-4, FR-5, FR-7): the delay (clock below 0), the emission window of `duration`
// (looping re-arms the bursts), bursts with cycles and intervals (at most one firing per burst per frame, counts hashed
// from seed, burst and cycle), start/stop and the queries built on them.

#include "particles/emitter_clock.hpp"

#include "particles/particle_random.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr std::int64_t kFrameTicks = FrameTime::kTicksPerFrame;

/// A burst is done when every cycle fired or its next firing is past the emission window.
void settle(BurstState& s, const ParticleBurst& d, std::int64_t duration) {
    if (d.cycles != 0 && s.fired >= d.cycles) s.done = true;
    if (duration > 0 && s.next >= duration) s.done = true;
}

} // namespace

void arm_bursts(ParticleBlock& b) {
    const std::int64_t duration = b.desc.duration.ticks;
    for (int i = 0; i < kMaxParticleBursts; ++i) {
        const ParticleBurst& d = b.desc.bursts[i];
        BurstState& s = b.bursts[i];
        s.used = d.count.max > 0;
        s.next = d.at.ticks;
        s.fired = 0;
        s.done = !s.used;
        settle(s, d, duration);
    }
}

void emission_start(ParticleBlock& b) {
    b.started = true;
    b.ended = false;
    b.clock = -static_cast<std::int64_t>(b.desc.delay.ticks);
    b.accumulator = 0;
    arm_bursts(b);
}

bool emission_over(const ParticleBlock& b) {
    if (!b.started || b.ended) return true;
    if (b.derived.rate) return false;
    if (b.desc.looping && b.desc.duration.ticks > 0) return false;
    for (const BurstState& s : b.bursts) {
        if (!s.done) return false;
    }
    return true;
}

ClockFrame clock_begin(const ParticleBlock& b) {
    ClockFrame f;
    if (!b.started || b.ended) return f;
    f.running = true;
    f.t0 = b.clock;
    f.t1 = b.clock + kFrameTicks;
    const std::int64_t duration = b.desc.duration.ticks;
    f.rate = b.derived.rate && f.t0 >= 0 && (duration == 0 || f.t0 < duration);
    return f;
}

void clock_end(ParticleBlock& b, const ClockFrame& f) {
    if (!f.running) return;
    const std::int64_t duration = b.desc.duration.ticks;
    for (int i = 0; i < kMaxParticleBursts; ++i) {
        BurstState& s = b.bursts[i];
        if (s.done || s.next >= f.t1) continue;
        const ParticleBurst& d = b.desc.bursts[i];
        const std::uint32_t span = static_cast<std::uint32_t>(d.count.max - d.count.min) + 1u;
        const int count = d.count.min + static_cast<int>(hash_below(burst_hash(b.seed, static_cast<std::uint32_t>(i),
                                                                                s.fired), span));
        emit_on(b, emitter_place(b), count);
        ++s.fired;
        s.next += d.interval.ticks;
        settle(s, d, duration);
    }
    if (duration > 0 && f.t1 >= duration) {
        if (b.desc.looping) {
            b.clock = f.t1 - duration;
            arm_bursts(b);
        } else {
            b.ended = true;
            b.clock = f.t1;
        }
    } else {
        b.clock = f.t1;
    }
}

} // namespace detail

void ParticleEmitter::start() {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::start on an empty emitter");
        return;
    }
    if (block_->started && !block_->ended) return;
    detail::emission_start(*block_);
}

void ParticleEmitter::stop() {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::stop on an empty emitter");
        return;
    }
    block_->started = false;
}

bool ParticleEmitter::emitting() const { return block_ != nullptr && block_->started && !block_->ended; }

bool ParticleEmitter::finished() const {
    return block_ != nullptr && detail::emission_over(*block_) && block_->alive == 0;
}

int ParticleEmitter::alive() const { return block_ != nullptr ? block_->alive : 0; }

} // namespace nnh
