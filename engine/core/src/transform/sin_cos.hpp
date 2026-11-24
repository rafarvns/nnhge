#pragma once
// The engine's sine and cosine (spec 022 FR-3, plan 022 "Deterministic sine"), inline for the hierarchy's pass and
// world_now (no call per node). Only IEEE + − × and float↔int conversions, in a fixed order, under -ffp-contract=off
// (every includer is a core float source): the same bits on every system. Signs and finiteness are integer tests on
// the bits (no float compare, no vmrs).

#include <bit>
#include <cstdint>

#include "core/det_sine.hpp"
#include "nnh/transform.hpp"

namespace nnh::detail {

inline constexpr std::uint32_t kExponentBits = 0x7F800000u;
inline constexpr std::uint32_t kSignBit = 0x80000000u;

inline std::uint32_t float_bits(float f) { return std::bit_cast<std::uint32_t>(f); }
inline bool finite_bits(std::uint32_t b) { return (b & kExponentBits) != kExponentBits; }

/// The quadrant (n & 3) of a whole q with |q| ≥ 2²³, from its mantissa bits: no float→int conversion can overflow.
NNH_DET_INLINE std::uint32_t huge_quadrant(std::uint32_t qbits) {
    const int shift = static_cast<int>((qbits >> 23) & 0xFFu) - 127 - 23; // q = mantissa · 2^shift, shift ≥ 0
    const std::uint32_t mantissa = (qbits & 0x7FFFFFu) | 0x800000u;
    const std::uint32_t low = shift >= 2 ? 0u : (mantissa << shift) & 3u;
    return (qbits & kSignBit) != 0 ? (0u - low) & 3u : low;
}

/// sin and cos of `angle` (radians; any finite value): q = angle · float(2/π), n = q rounded to the nearest whole by
/// ±0.5 and truncation, f = q − n (exact), |f| < 2⁻²⁰ snapped to 0, then the quadrant n & 3 picks ±sin_quarter(|f|)
/// and ±sin_quarter(1 − |f|). A non-finite angle gives {0, 1}.
NNH_DET_INLINE SinCos sin_cos_inline(float angle) {
    if (!finite_bits(float_bits(angle))) return {0.0f, 1.0f};
    const float q = angle * 0.636619747f; // float(2/π): one rounding
    const std::uint32_t qbits = float_bits(q);
    std::uint32_t quadrant;
    float f;
    if ((qbits & ~kSignBit) >= 0x4B000000u) { // |q| ≥ 2²³: q is whole
        quadrant = huge_quadrant(qbits);
        f = 0.0f;
    } else {
        const float half = (qbits & kSignBit) != 0 ? -0.5f : 0.5f;
        const auto n = static_cast<std::int32_t>(q + half); // truncation: the nearest whole (ties away from zero)
        f = q - static_cast<float>(n);                       // exact
        quadrant = static_cast<std::uint32_t>(n) & 3u;
        if ((float_bits(f) & ~kSignBit) < 0x35800000u) f = 0.0f * f; // |f| < 2⁻²⁰: a whole quarter (sign kept)
    }
    const std::uint32_t fsign = float_bits(f) & kSignBit;
    const float a = std::bit_cast<float>(float_bits(f) & ~kSignBit); // |f|
    const float s0 = std::bit_cast<float>(float_bits(sin_quarter_inline(a)) | fsign); // sin(π/2·f)
    const float c0 = sin_quarter_inline(1.0f - a);                                    // cos(π/2·f)
    switch (quadrant) {
    case 0:
        return {s0, c0};
    case 1:
        return {c0, -s0};
    case 2:
        return {-s0, -c0};
    default:
        return {-c0, s0};
    }
}

} // namespace nnh::detail
