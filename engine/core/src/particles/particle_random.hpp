#pragma once
// The particles' counter-based generator (spec 024 FR-15, plan "Determinism"): every random value of particle `n` is a
// hash of the emitter's seed, `n` and a fixed property index, so values never depend on another emitter, on drops, on
// culling or on draw order, and restart() replays them. Integers only, then one float conversion.

#include <cstdint>

#include "core/mix32.hpp"

namespace nnh::detail {

/// Property indexes: the order values are drawn in a birth (a new property is appended, never inserted).
enum ParticleProperty : std::uint32_t {
    kPropShapeA = 0,
    kPropShapeB = 1,
    kPropShapeC = 2,
    kPropDirection = 3,
    kPropSpeed = 4,
    kPropRotation = 5,
    kPropSpin = 6,
    kPropLifetime = 7,
    kPropColor = 8,
    kPropSize = 9,
    kPropFrame = 10,
    kPropShapeD = 11,
};

inline constexpr std::uint32_t kGoldenGamma = 0x9E3779B9u;

/// The base hash of particle number `n` of an emitter seeded `seed`.
inline std::uint32_t particle_base(std::uint32_t seed, std::uint32_t n) { return mix32(seed ^ mix32(n)); }

/// The full 32-bit value of property `k`.
inline std::uint32_t particle_hash(std::uint32_t base, std::uint32_t k) { return mix32(base + k * kGoldenGamma); }

/// A float in [0, 1): the hash's top 24 bits × 2⁻²⁴ (exact).
inline float unit_float(std::uint32_t h) { return static_cast<float>(h >> 8) * (1.0f / 16777216.0f); }

/// An integer in [0, count) from a full hash: count × h >> 32 (one 32×32→64 multiply, no division). count ≥ 1.
inline std::uint32_t hash_below(std::uint32_t h, std::uint32_t count) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(count) * h >> 32);
}

/// A burst's count hash: (seed, burst, cycle), apart from the particles' numbers.
inline std::uint32_t burst_hash(std::uint32_t seed, std::uint32_t burst, std::uint32_t cycle) {
    return mix32(seed ^ mix32(0xB0057000u + burst) ^ mix32(cycle * kGoldenGamma + 0x51ED2701u));
}

} // namespace nnh::detail
