#pragma once
// The particles self-check (spec 024 AC-2, AC-6): 8 emitters driven by a fixed script for 3600 frames, every value of
// every live particle hashed every 600 frames. Pure (no drawing): compiled into the example (PARTICLES_DEMO=check and
// the console script) and into nnh_tests, where the golden must hold on every host compiler.

#include <cstdint>

#include "nnh/particles.hpp"

namespace particles_check {

inline constexpr int kEmitters = 8;
inline constexpr int kFrames = 3600;
inline constexpr int kCheckpointEvery = 600;
inline constexpr int kCheckpoints = kFrames / kCheckpointEvery;

/// Goldens recorded from pc-debug (plan 024 "Stage 5 results"); equal on every compiler and on the 3DS.
inline constexpr std::uint32_t kParticlesCheckpoints[kCheckpoints] = {0xD39517FFu, 0x640E6215u, 0x35C3F348u,
                                                                      0x2E105955u, 0x6BA1B564u, 0xC5EF1A84u};
inline constexpr std::uint32_t kParticlesCheckGolden = 0xC5EF1A84u;

/// E6's description: circle area, random frames (the `sparks` preset, assets/fx/sparks.particles).
nnh::ParticleDesc sparks_desc();

/// The script: create the emitters (once), then step() frame by frame.
class SelfCheck {
public:
    /// Creates the emitters; `e6` replaces E6's description (the preset test). False if creation failed.
    bool begin(const nnh::ParticleDesc* e6 = nullptr);
    /// Runs one frame of the script; returns true on a checkpoint frame (checksum() is then that checkpoint's).
    bool step();
    int frame() const { return frame_; }
    bool done() const { return frame_ >= kFrames; }
    std::uint32_t checksum() const { return sum_; }
    /// The FNV-1a of the current state of every emitter (counters and every live particle's view).
    std::uint32_t hash_now() const;
    const nnh::ParticleEmitter& emitter(int i) const { return emitters_[i]; }
    int total_alive() const;

private:
    void script(int f);
    nnh::ParticleEmitter emitters_[kEmitters];
    int frame_ = 0;
    std::uint32_t sum_ = 0;
};

} // namespace particles_check
