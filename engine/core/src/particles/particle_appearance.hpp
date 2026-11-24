#pragma once
// A particle's appearance as a function of its age (spec 024 FR-12, FR-13, plan "Appearance"), shared by particle()
// and the draw expansion: t = age / life; the curve segment from a 256-bucket table then a short forward walk; channels
// rounded and multiplied by the start colour exactly (mul8); one branch-free frame rule for every mode. Comparisons
// are on the bits of non-negative floats (no vmrs). Included only by -ffp-contract=off sources.

#include <bit>
#include <cstdint>

#include "particles/particle_block.hpp"

namespace nnh::detail {

/// round(a · b / 255) for bytes, exactly, in integers.
inline std::uint32_t mul8(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t t = a * b + 128u;
    return (t + (t >> 8)) >> 8;
}

/// Packed RGBA8 (r in the low byte) multiplied channel by channel.
inline std::uint32_t mul_color(std::uint32_t a, std::uint32_t b) {
    std::uint32_t out = 0;
    for (int k = 0; k < 32; k += 8) out |= mul8((a >> k) & 0xFFu, (b >> k) & 0xFFu) << k;
    return out;
}

inline std::uint32_t pack_color(Color c) {
    return static_cast<std::uint32_t>(c.r) | (static_cast<std::uint32_t>(c.g) << 8) |
           (static_cast<std::uint32_t>(c.b) << 16) | (static_cast<std::uint32_t>(c.a) << 24);
}

inline Color unpack_color(std::uint32_t c) {
    return {static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(c >> 8), static_cast<std::uint8_t>(c >> 16),
            static_cast<std::uint8_t>(c >> 24)};
}

/// The fraction of its life a particle has lived: t ≥ 0 (and < 1 up to rounding).
inline float life_fraction(const ParticleHot& h, const ParticleCold& c) {
    return static_cast<float>(h.age) * c.inv_life;
}

/// The segment holding `t` (t ≥ 0; starts are ≥ 0 and the sentinel 2, so their bits order as their values).
template <int Channels>
inline int curve_segment(const ParticleCurve<Channels>& c, float t) {
    const std::uint32_t tb = std::bit_cast<std::uint32_t>(t);
    const auto i = static_cast<std::uint32_t>(t * 256.0f);
    int s = c.bucket[i > 255u ? 255u : i];
    while (tb >= std::bit_cast<std::uint32_t>(c.start[s + 1])) ++s;
    return s;
}

/// The gradient's colour at `t` times the start colour `start` (both packed).
inline std::uint32_t color_at(const ParticleColorCurve& c, float t, std::uint32_t start) {
    const int s = curve_segment(c, t);
    const float frac = (t - c.start[s]) * c.inv_len[s];
    std::uint32_t out = 0;
    for (int k = 0; k < 4; ++k) {
        const auto v = static_cast<std::uint32_t>(c.v0[s][k] + c.dv[s][k] * frac + 0.5f);
        out |= mul8(v, (start >> (8 * k)) & 0xFFu) << (8 * k);
    }
    return out;
}

/// The size curve's multiplier at `t` times the start size.
inline float size_at(const ParticleSizeCurve& c, float t, float start) {
    const int s = curve_segment(c, t);
    const float frac = (t - c.start[s]) * c.inv_len[s];
    return start * (c.v0[s][0] + c.dv[s][0] * frac);
}

/// m mod n for m < 2²⁴ and n ≤ 16 through the 28-bit reciprocal (exact: proven exhaustively by a test).
inline std::uint32_t frame_mod(std::uint32_t m, std::uint32_t n, std::uint32_t recip) {
    const auto q = static_cast<std::uint32_t>(static_cast<std::uint64_t>(m) * recip >> 28);
    return m - n * q;
}

/// The frame shown at age `age` (t its life fraction): base + age·kA + t·kB, clamped, mod n.
inline int frame_at(const ParticleDerived& d, const ParticleHot& h, const ParticleCold& c, float t) {
    const std::uint32_t base = d.random_frame ? c.frame : 0u;
    std::uint32_t m = base + static_cast<std::uint32_t>(static_cast<float>(h.age) * d.frame_per_tick +
                                                         t * d.frame_per_life);
    m = m < d.frame_last ? m : d.frame_last;
    return static_cast<int>(frame_mod(m, static_cast<std::uint32_t>(d.frames), d.frame_recip));
}

} // namespace nnh::detail
