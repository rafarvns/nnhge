#pragma once
// AC-7's workloads (spec 024), pure: each call is one frame of work that bench_scene.cpp times. update1024 (one emitter
// of 1024 live particles: rate, gravity, drag, spin, a 4-key gradient, a 3-key size curve, frames over life),
// update16x64 (the same particles in 16 emitters), emit512 (32 emit_at bursts of 16 into a cleared 8192 emitter),
// draw512 / sprite512 (512 frozen visible particles, or 512 gfx::draw sprites with the same frames, rotations and
// tints), offscreen (512 particles outside the view), frame (512 simulated and drawn + 300 16x16 sprites).

#include <nnh/particles.hpp>
#include <nnh/texture.hpp>

class ParticleBench {
public:
    static constexpr int kDrawn = 512;
    static constexpr int kSprites = 300;

    /// Creates every emitter and prewarms them. False when out of memory.
    bool init(const nnh::Texture* sheet);

    void update1024();
    void update16x64();
    void clear_emit512(); ///< untimed: the 8192 emitter emptied before each emit512 frame
    int emit512();
    void draw512() const;   ///< the frozen emitter's one draw call (its expansion is in gfx submit)
    void sprite512() const; ///< 512 gfx::draw calls equal to draw512's particles
    void offscreen() const; ///< the frozen emitter drawn far outside the view
    void frame(int n);      ///< 512 simulated + drawn, 300 sprites
    int culled() const { return frozen_.stats().culled; }

private:
    const nnh::Texture* sheet_ = nullptr;
    nnh::ParticleEmitter one_;
    nnh::ParticleEmitter many_[16];
    nnh::ParticleEmitter burst_;
    nnh::ParticleEmitter frozen_;
    nnh::ParticleEmitter live_;
    std::uint32_t seed_ = 1;
};
