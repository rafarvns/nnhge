#pragma once
// Births shared by emission on demand, bursts and the rate (emitter_emit.cpp, emitter_rate.cpp).

#include <cstdint>

#include "particles/particle_block.hpp"
#include "particles/particle_map.hpp"

namespace nnh::detail {

/// Where a birth happens: shape-local → stored space (world, or the emitter's frame for local space).
struct BirthMap {
    Affine2D map{};
    bool translation = true; ///< the map's axes are the identity: positions are moved only
    float rotation = 0;      ///< added to the particle's rotation (by the mirror rule)
    std::uint32_t mirror = 0;
    Vec2 inherit{}; ///< inherit_velocity × the emitter's velocity (world space), px per frame
};

/// The map of transform `at` for emitter `b`.
BirthMap birth_map(const ParticleBlock& b, const WorldTransform2D& at);

/// How many of `count` particles fit; the rest are counted as dropped (and warned about once in Debug/Development).
int reserve_births(ParticleBlock& b, int count);

/// Writes particle number `number` at index alive (room must exist), pre-aged by `pre_age` of a frame (0..1).
void birth(ParticleBlock& b, std::uint32_t number, const BirthMap& at, float pre_age);

} // namespace nnh::detail
