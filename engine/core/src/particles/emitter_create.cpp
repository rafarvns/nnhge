// Emitters (spec 024 FR-1, FR-2, FR-7): description checks, one block of tagged memory (header, hot and cold records),
// moves, destruction, set_desc/set_seed and the prewarm at creation.

#include <new>

#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "particles/particle_block.hpp"
#include "particles/particle_desc_check.hpp"

NNH_MEM_TAG(particles);
NNH_LOG_CHANNEL(particles);

namespace nnh {
namespace detail {

MemTag& particle_memory_tag() { return NNH_MEM_TAG_REF(particles); }

void particle_misuse(const char* what) {
    NNH_LOG_ERROR(particles, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

void particle_unbind(ParticleBlock& b); // emitter_scene.cpp
void gfx_forget_particles(const void* source); // gfx/particle_batches.cpp

namespace {

void prewarm(ParticleBlock& b) {
    b.before = b.now;
    for (int i = 0; i < b.desc.prewarm; ++i) emitter_step(b);
}

/// Burst states for a replaced description: indexes keep their clock; a burst that becomes used is armed.
void rearm_changed_bursts(ParticleBlock& b) {
    for (int i = 0; i < kMaxParticleBursts; ++i) {
        const ParticleBurst& d = b.desc.bursts[i];
        BurstState& s = b.bursts[i];
        const bool used = d.count.max > 0;
        if (used && !s.used) {
            s.next = d.at.ticks;
            s.fired = 0;
            s.done = false;
        }
        s.used = used;
        if (!used) s.done = true;
    }
}

} // namespace
} // namespace detail

Result<void> ParticleEmitter::check(const ParticleDesc& desc) {
    const Error e = detail::check_particle_desc(desc);
    if (e.code != ErrorCode::None) return e;
    return {};
}

Result<ParticleEmitter> ParticleEmitter::create(const ParticleDesc& desc, int capacity, MemTag* tag) {
    const Error e = detail::check_particle_desc(desc);
    if (e.code != ErrorCode::None) return e;
    if (capacity < 1 || capacity > kMaxParticles) {
        return Error::make(ErrorCode::InvalidArgument, "particle capacity %d: must be 1..%d", capacity, kMaxParticles);
    }
    const std::size_t bytes = detail::particle_block_bytes(capacity);
    MemTag& t = tag != nullptr ? *tag : detail::particle_memory_tag();
    auto* raw = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, t, bytes, 32));
    if (raw == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "particles: no memory for %d particles (%zu bytes)", capacity, bytes);
    }
    auto* b = ::new (raw) detail::ParticleBlock{};
    // Implicit-lifetime records (trivially copyable): always written at birth before they are read.
    unsigned char* records = raw + detail::kParticleHeaderBytes;
    b->hot = reinterpret_cast<detail::ParticleHot*>(records);
    b->cold = reinterpret_cast<detail::ParticleCold*>(records + sizeof(detail::ParticleHot) *
                                                                   static_cast<std::size_t>(capacity));
    b->capacity = capacity;
    b->desc = desc;
    b->seed = desc.seed;
    detail::particle_derive(desc, b->derived);
    detail::emission_start(*b);
    detail::prewarm(*b);
    ParticleEmitter emitter;
    emitter.block_ = b;
    return emitter;
}

ParticleEmitter::ParticleEmitter(ParticleEmitter&& other) noexcept : block_(other.block_) { other.block_ = nullptr; }

ParticleEmitter& ParticleEmitter::operator=(ParticleEmitter&& other) noexcept {
    if (this != &other) {
        ParticleEmitter gone(static_cast<ParticleEmitter&&>(*this));
        block_ = other.block_;
        other.block_ = nullptr;
    }
    return *this;
}

ParticleEmitter::~ParticleEmitter() {
    detail::ParticleBlock* b = block_;
    if (b == nullptr) return;
    block_ = nullptr;
    detail::particle_unbind(*b);
    detail::gfx_forget_particles(b); // this frame's draws of it draw nothing
    b->~ParticleBlock();
    mem::free(b);
}

bool ParticleEmitter::valid() const { return block_ != nullptr; }

const ParticleDesc& ParticleEmitter::desc() const {
    static const ParticleDesc kEmpty{};
    return block_ != nullptr ? block_->desc : kEmpty;
}

void ParticleEmitter::set_desc(const ParticleDesc& desc) {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::set_desc on an empty emitter");
        return;
    }
    const Error e = detail::check_particle_desc(desc);
    if (e.code != ErrorCode::None) {
        NNH_PARTICLE_MISUSE(e.message);
        return;
    }
    block_->desc = desc;
    block_->seed = desc.seed;
    detail::particle_derive(desc, block_->derived);
    detail::rearm_changed_bursts(*block_);
}

void ParticleEmitter::set_seed(std::uint32_t seed) {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::set_seed on an empty emitter");
        return;
    }
    block_->seed = seed;
}

void ParticleEmitter::restart() {
    detail::ParticleBlock* b = block_;
    if (b == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::restart on an empty emitter");
        return;
    }
    detail::emitter_clear(*b);
    b->next_number = 0;
    b->emitted = 0;
    b->dropped = 0;
    b->warned_full = false;
    b->peak = 0;
    b->emitter_velocity = {};
    b->started = false;
    detail::emission_start(*b);
    detail::prewarm(*b);
}

} // namespace nnh
