#pragma once
// An emitter's block (spec 024 D39): a header (state, clock, counters, transforms, the scene attachment, stats), the
// description and its derived tables, then `capacity` hot records (32 B: what update() reads and writes) and `capacity`
// cold ones (16 B: written at birth, moved by compaction), both in emission order. Appearance (colour, size, frame) is
// a function of age, evaluated when drawn or queried, never stored.

#include <cstddef>
#include <cstdint>

#include "nnh/particles.hpp"
#include "particles/particle_derived.hpp"
#include "scene/scene_attachments.hpp"

namespace nnh {
struct MemTag;
}

namespace nnh::detail {

/// What update() touches for every particle: one 32-byte cache line.
struct alignas(32) ParticleHot {
    float px = 0, py = 0; ///< position (world, or local for a local-space emitter)
    float vx = 0, vy = 0; ///< velocity, px per frame
    float rotation = 0;   ///< radians
    float spin = 0;       ///< radians per frame
    std::uint32_t age = 0;  ///< frame-clock ticks
    std::uint32_t life = 0; ///< ticks; dies when age reaches it
};
static_assert(sizeof(ParticleHot) == 32, "one cache line per particle");

/// Written at birth, read when drawn or queried.
struct ParticleCold {
    float inv_life = 0;      ///< 1 / life (ticks)
    float size = 0;          ///< start size
    std::uint32_t color = 0; ///< start colour, RGBA8 (r in the low byte)
    std::uint8_t frame = 0;  ///< Random: the frame chosen at birth
    std::uint8_t spare[3] = {};
};
static_assert(sizeof(ParticleCold) == 16, "16 bytes per particle");

/// A burst's state on the emitter's clock.
struct BurstState {
    std::int64_t next = 0;    ///< clock tick of its next firing
    std::uint32_t fired = 0;  ///< cycles fired since the emission (re)started
    bool used = false;        ///< count.max > 0
    bool done = false;        ///< every cycle fired (or past the duration)
};

struct ParticleBlock {
    ParticleHot* hot = nullptr;
    ParticleCold* cold = nullptr;
    int capacity = 0;
    int alive = 0;
    int peak = 0;
    std::uint32_t seed = 1;
    std::uint32_t next_number = 0; ///< emission number of the next particle asked for (dropped ones take theirs)
    std::uint32_t emitted = 0;
    std::uint32_t dropped = 0;
    bool started = false;
    bool ended = false;
    bool warned_full = false;
    std::int64_t clock = 0;        ///< ticks since the start of the emission; negative during the delay
    std::uint32_t accumulator = 0; ///< rate: the next particle's fraction, in 1/(60 × 65536) (< kRateThreshold)
    BurstState bursts[kMaxParticleBursts] = {};
    WorldTransform2D now{};
    WorldTransform2D before{};      ///< the transform at the end of the previous update
    Vec2 emitter_velocity{};        ///< origin change over the last update, px per frame
    std::int32_t key_min_x = 0, key_min_y = 0, key_max_x = 0, key_max_y = 0; ///< bounds (order keys), alive > 0
    int drawn = 0;
    int culled = 0;
    std::uint32_t stats_frame = 0xFFFFFFFFu;
    /// The frames' cull reach for a texture size (spec 024 T10.3): found by the first textured expansion with that size,
    /// reused while the description stays (set_desc forgets it), so an emitter culled whole reads no frame.
    std::int32_t reach_texture_w = -1, reach_texture_h = -1; ///< -1: not found yet
    float frames_reach = 0;
    bool frames_fit = false; ///< every frame's region lies inside that texture
    Scene* scene = nullptr;
    SceneAttachment attachment{};
    ParticleDesc desc{};
    ParticleDerived derived{};
};

/// Bytes of the header, rounded to the records' alignment.
inline constexpr std::size_t kParticleHeaderBytes = (sizeof(ParticleBlock) + 31u) & ~std::size_t{31};

/// The block's size for `capacity` particles: header + 48 × capacity.
constexpr std::size_t particle_block_bytes(int capacity) {
    return kParticleHeaderBytes + static_cast<std::size_t>(capacity) * (sizeof(ParticleHot) + sizeof(ParticleCold));
}

/// The engine's default tag for emitters ("particles").
MemTag& particle_memory_tag();

/// Misuse: an error log and an assert (Debug/Development). Use NNH_PARTICLE_MISUSE: Release keeps no text.
void particle_misuse(const char* what);

/// The rest of the emitter's internals, shared by its files.
void emission_start(ParticleBlock& b);             ///< emitting from the clock's start: delay, bursts armed
void arm_bursts(ParticleBlock& b);                 ///< bursts from their first firing
bool emission_over(const ParticleBlock& b);        ///< no rate or burst emission left
void emitter_step(ParticleBlock& b);               ///< update(): one frame
void emitter_clear(ParticleBlock& b);              ///< no particle, bounds empty
/// Creates up to `count` particles at age 0 on `map`/`rotation`/`mirror` (world space) or in local space (`local_at`
/// applied). @return the particles created.
int emit_on(ParticleBlock& b, const WorldTransform2D& at, int count);

/// Every float of `t` finite (an integer test).
bool finite_world(const WorldTransform2D& t);

/// Where the emitter's own births happen in its stored space: its transform (world space) or the identity (local).
inline const WorldTransform2D& emitter_place(const ParticleBlock& b) {
    static constexpr WorldTransform2D kIdentity{};
    return b.derived.world ? b.now : kIdentity;
}

} // namespace nnh::detail

#if defined(NNH_FLAVOR_RELEASE)
#define NNH_PARTICLE_MISUSE(what) ((void)0)
#else
#define NNH_PARTICLE_MISUSE(what) ::nnh::detail::particle_misuse(what)
#endif

namespace nnh {
/// Reaches an emitter's block (friend of ParticleEmitter).
struct ParticleAccess {
    static detail::ParticleBlock* block(const ParticleEmitter& e) { return e.block_; }
};
} // namespace nnh
