#pragma once
// sin_cos_inline without a branch (spec 024 T10.3, for the particles' births): the same bits for every angle,
// every case (non-finite, |q| ≥ 2²³, the snap, the quadrant) computed and picked by integer masks, and the
// clamp of the quarter sine as an integer compare (no vmrs). The work is split in stages so that
// sin_cos_flat2 can run two angles side by side: on the in-order ARM11 (VFP latency 8 cycles) two sines
// written lane by lane cost little more than one. Only IEEE + − × and float↔int conversions in
// sin_cos_inline's order, under -ffp-contract=off (core float sources).

#include "transform/sin_cos.hpp"

#include <bit>
#include <cstdint>

namespace nnh::detail {

/// All ones when `condition` holds, else zero; opaque to the optimiser, so a select built on it stays a
/// select (GCC otherwise turns these selects back into branches and duplicates the paths).
NNH_DET_INLINE std::uint32_t select_mask(bool condition) {
    std::uint32_t m = 0u - static_cast<std::uint32_t>(condition);
#if defined(__GNUC__) || defined(__clang__)
    __asm__("" : "+r"(m));
#endif
    return m;
}

/// `a` where `mask` is set, else `b`.
NNH_DET_INLINE std::uint32_t select_bits(std::uint32_t mask, std::uint32_t a, std::uint32_t b) {
    return (a & mask) | (b & ~mask);
}

/// One angle's state between the stages.
struct FlatSine {
    std::uint32_t finite; ///< mask: the angle is finite
    std::uint32_t qbits;  ///< angle × 2/π
    std::uint32_t huge;   ///< mask: |q| ≥ 2²³ (q is whole)
    std::int32_t n;       ///< q rounded to the nearest whole
    std::uint32_t quadrant;
    std::uint32_t fsign;
    float a; ///< |f|, f = q − n snapped
    float s; ///< sin_quarter(a), unclamped
    float c; ///< sin_quarter(1 − a), unclamped
};

/// q = angle · float(2/π) (a non-finite angle as 0) and its masks.
NNH_DET_INLINE void flat_reduce(float angle, FlatSine& x) {
    const std::uint32_t abits = float_bits(angle);
    x.finite = select_mask(finite_bits(abits));
    x.qbits = float_bits(std::bit_cast<float>(abits & x.finite) * 0.636619747f);
    x.huge = select_mask((x.qbits & ~kSignBit) >= 0x4B000000u);
}

/// n by ±0.5 and truncation (q as 0 when huge: in range), f = q − n exact, |f| < 2⁻²⁰ snapped to a signed
/// zero, the quadrant (a huge q's from its mantissa: a shift of 2 or more leaves 0).
NNH_DET_INLINE void flat_split(FlatSine& x) {
    const float qs = std::bit_cast<float>(x.qbits & ~x.huge);
    const float half = std::bit_cast<float>((x.qbits & kSignBit) | 0x3F000000u);
    x.n = static_cast<std::int32_t>(qs + half);
    std::uint32_t fb = float_bits(qs - static_cast<float>(x.n));
    fb = select_bits(select_mask((fb & ~kSignBit) < 0x35800000u), fb & kSignBit, fb);
    fb &= ~x.huge;
    const std::uint32_t whole_bits = select_bits(x.huge, x.qbits, 0x4B000000u);
    const int shift = static_cast<int>((whole_bits >> 23) & 0xFFu) - 150; // ≥ 0
    const std::uint32_t low = (((whole_bits & 0x7FFFFFu) | 0x800000u) << (shift < 2 ? shift : 2)) & 3u;
    const std::uint32_t qneg = 0u - (x.qbits >> 31);
    x.quadrant = select_bits(x.huge, ((low ^ qneg) - qneg) & 3u, static_cast<std::uint32_t>(x.n) & 3u);
    x.fsign = fb & kSignBit;
    x.a = std::bit_cast<float>(fb & ~kSignBit);
}

/// The two quarter-sine polynomials (sin_quarter_inline's, before its clamp).
NNH_DET_INLINE void flat_poly(FlatSine& x) {
    const float u = x.a, w = 1.0f - x.a;
    const float u2 = u * u, w2 = w * w;
    x.s = u * (1.5707903f + u2 * (-0.64588609f + u2 * (0.079418352f + u2 * -0.0043225876f)));
    x.c = w * (1.5707903f + w2 * (-0.64588609f + w2 * (0.079418352f + w2 * -0.0043225876f)));
}

/// The clamp below 1 (both values are non-negative and finite: an integer compare), the quadrant's swap and
/// signs.
NNH_DET_INLINE SinCos flat_finish(const FlatSine& x) {
    const std::uint32_t sv = float_bits(x.s), cv = float_bits(x.c);
    const std::uint32_t s0 = select_bits(select_mask(sv < 0x3F800000u), sv, 0x3F800000u) | x.fsign;
    const std::uint32_t c0 = select_bits(select_mask(cv < 0x3F800000u), cv, 0x3F800000u);
    const std::uint32_t q = x.quadrant;
    const std::uint32_t swap = 0u - (q & 1u);                  // quadrants 1, 3: sin ← ±c0, cos ← ±s0
    const std::uint32_t sin_neg = (q >> 1) << 31;              // quadrants 2 and 3
    const std::uint32_t cos_neg = ((q ^ (q >> 1)) & 1u) << 31; // quadrants 1 and 2
    const std::uint32_t sb = select_bits(swap, c0, s0) ^ sin_neg;
    const std::uint32_t cb = select_bits(swap, s0, c0) ^ cos_neg;
    return {std::bit_cast<float>(sb & x.finite),
            std::bit_cast<float>(select_bits(x.finite, cb, 0x3F800000u))};
}

/// sin_cos_inline(angle), bit for bit, without a branch.
NNH_DET_INLINE SinCos sin_cos_flat(float angle) {
    FlatSine x;
    flat_reduce(angle, x);
    flat_split(x);
    flat_poly(x);
    return flat_finish(x);
}

/// Two angles lane by lane (their dependency chains interleave): sin_cos_flat(a) and sin_cos_flat(b).
NNH_DET_INLINE void sin_cos_flat2(float a, float b, SinCos& sa, SinCos& sb) {
    FlatSine x, y;
    flat_reduce(a, x);
    flat_reduce(b, y);
    flat_split(x);
    flat_split(y);
    flat_poly(x);
    flat_poly(y);
    sa = flat_finish(x);
    sb = flat_finish(y);
}

} // namespace nnh::detail
