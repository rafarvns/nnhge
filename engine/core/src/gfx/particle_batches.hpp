#pragma once
// The frame's particle batches (spec 024, decision D39; D26 generalised). gfx knows a particle batch only as {expand
// function, source}: it never includes the particles module. ParticleEmitter::draw records one batch plus one draw-list
// command whose tile_batch index is kParticleBatchBase + i, so it keeps its place among the sprites and tile layers. At
// submit, gfx_frame.cpp reaches this code only through g_gfx_particle_ops, set by a registrar in particle_batches.cpp
// (linked exactly when a game draws or destroys an emitter): the expand function writes the visible particles' 2D items
// into a 128-item scratch, chunk by chunk, within the frame's max_particles, and each chunk goes to platform::draw_2d.

#include <cstdint>

#include "gfx/tile_batches.hpp"
#include "nnh/gfx.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::detail {

/// Particle batches per frame (both screens).
inline constexpr int kMaxParticleBatches = 32;
/// A draw command's tile_batch at or above this is particle batch (tile_batch − kParticleBatchBase).
inline constexpr int kParticleBatchBase = kMaxTileBatches;
/// Items expanded per draw_2d call.
inline constexpr int kParticleScratchItems = 128;

struct ParticleExpandRequest {
    const void* source = nullptr; ///< what recorded the batch (an emitter's block)
    Screen screen = Screen::Top;
    Vec2 camera{};  ///< the screen's camera at the end of the frame; zero when ignoring it
    float zoom = 1; ///< 1 when ignoring the camera
    platform::TextureHandle texture = 0; ///< textured: the resolved texture
    bool textured = false;
    int texture_w = 0, texture_h = 0;
    std::uint32_t tint = 0xFFFFFFFFu; ///< RGBA8 (r low), alpha already × opacity
    std::uint8_t layer = 0;
    std::uint32_t frame = 0; ///< gfx's frame number (the emitter's drawn/culled stats restart with a new frame)
    int first = 0;           ///< resume at this draw-order index
    platform::DrawItem2D* out = nullptr;
    int capacity = 0; ///< items `out` can take; 0: the budget is spent, count the visible ones as dropped
};

struct ParticleExpandResult {
    int written = 0;
    int culled = 0;
    int dropped = 0;  ///< visible particles counted with capacity 0
    int next = -1;    ///< ≥ 0: `out` filled, call again from this index; -1: done
    bool bad_frame = false; ///< a frame's region lies outside the texture: nothing drawn
};

using ParticleExpandFn = ParticleExpandResult (*)(const ParticleExpandRequest& request);

struct ParticleBatchRecord {
    ParticleExpandFn expand = nullptr; ///< nullptr: forgotten (its source was destroyed)
    const void* source = nullptr;
    int texture_slot = -1; ///< -1: texture-less squares; else resolved at submit (stale = skipped)
    std::uint32_t texture_generation = 0;
    std::uint32_t tint = 0xFFFFFFFFu;
    std::uint8_t layer = 0;
    bool ignore_camera = false;
};

/// The table, scratch and budget (pure: storage given by the caller; unit tested).
class ParticleBatches {
public:
    using SendFn = void (*)(Screen screen, const platform::DrawItem2D* items, int count);

    void reset(ParticleBatchRecord* records, int capacity, platform::DrawItem2D* scratch, int scratch_capacity,
               int max_particles);
    void clear(); ///< a new frame: no batches, nothing counted
    int add(const ParticleBatchRecord& record); ///< the batch's index, or -1 when the frame's batches are full
    const ParticleBatchRecord* get(int index) const; ///< nullptr for a forgotten batch or a bad index
    void forget(const void* source);
    /// Expands batch `index` (its texture already resolved into `texture`, or none) in chunks sent with `send`.
    /// @return false when a frame region lies outside the texture (nothing drawn).
    bool submit(int index, Screen screen, Camera2D camera, bool textured, platform::TextureHandle texture, int tw,
                int th, std::uint32_t frame, SendFn send);

    int count() const { return count_; }
    int max_particles() const { return max_; }
    int drawn() const { return drawn_; }     ///< particles drawn this frame
    int dropped() const { return dropped_; } ///< particles over the budget this frame
    /// Going back to an earlier point of the frame's budget (a stereo frame's second eye, spec 026 FR-15).
    void rewind(int drawn, int dropped) { drawn_ = drawn, dropped_ = dropped; }

private:
    ParticleBatchRecord* records_ = nullptr;
    platform::DrawItem2D* scratch_ = nullptr;
    int capacity_ = 0;
    int scratch_capacity_ = 0;
    int max_ = 0;
    int count_ = 0;
    int drawn_ = 0;
    int dropped_ = 0;
};

/// gfx's hooks into the particle batches: null unless particle_batches.cpp is linked (D39, the optional frame hook).
struct GfxParticleOps {
    bool (*init)(int max_particles);                        ///< allocates when max_particles > 0
    void (*shutdown)();                                     ///< frees
    void (*submit)(Screen screen, int index, Camera2D camera); ///< a texture gone: asserts, skipped
    void (*end_frame)();                                    ///< counts, warns about drops, clears the table
    void (*stats)(gfx::Stats& out);                         ///< the particle fields
    /// Stereo (spec 026 FR-15): remember the budget spent before the top screen's first eye, and go back to it before
    /// the second, so both eyes draw the same particles and the budget is spent once.
    void (*budget_mark)();
    void (*budget_rewind)();
};
extern const GfxParticleOps* g_gfx_particle_ops;

/// Records a particle batch on `screen` (ParticleEmitter::draw): asserts (and records nothing in Release) outside a
/// frame, when max_particles is 0 (Release: one warning per init) or with kMaxParticleBatches batches already.
void gfx_record_particles(Screen screen, int layer, const ParticleBatchRecord& record);
/// Forgets this frame's batches of `source` (an emitter being destroyed).
void gfx_forget_particles(const void* source);

} // namespace nnh::detail
