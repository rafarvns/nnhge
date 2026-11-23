#pragma once
// The 1/256 px lattice (plan "Console bench refinement (round 2)"): a coordinate that is a multiple of 1/256
// px with |v| < 2^14 px is held as the integer v · 256 (|.| < 2^22). Sums of a few such values and their
// products stay exact in int32 / int64 and in float / double alike (D34's exact range), so integer code on
// lattice inputs makes the same decisions as the double code; the walk and the slab test use it to step
// without VFP compares on the 3DS. Internal, pure.

#include <cstdint>
#include <cstring>

namespace nnh::collision::detail {

inline constexpr int kLatticeBits = 8;              ///< 1/256 px
inline constexpr double kLatticeUnit = 1.0 / 256.0; ///< one step of the lattice, in px

/// `v` · 256 into `out` when `v` is on the lattice (a multiple of 1/256, |v| < 2^14, ±0 → 0); false otherwise
/// (NaN, infinities, subnormals included). Integer only: the float's bits, no VFP compare.
inline bool to_lattice(float v, std::int32_t& out) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    const std::uint32_t magnitude = b & 0x7FFFFFFFu;
    if (magnitude == 0) {
        out = 0;
        return true;
    }
    // v = m · 2^(e − 150) with m = 1.f as a 24-bit integer, so v · 256 = m · 2^(e − 142):
    // |v| < 2^14 ⇔ e ≤ 140; a multiple of 1/256 ⇔ the low 142 − e bits of m are zero
    // (none can be when 142 − e > 23: then 0 < |v| < 2^-8).
    const int e = static_cast<int>(magnitude >> 23);
    const int shift = 142 - e;
    if (e == 0 || shift < 2 || shift > 23) return false;
    const std::uint32_t m = (magnitude & 0x7FFFFFu) | 0x800000u;
    if ((m & ((1u << shift) - 1u)) != 0) return false;
    const std::int32_t units = static_cast<std::int32_t>(m >> shift);
    out = (b & 0x80000000u) != 0 ? -units : units;
    return true;
}

/// −1, 0 or 1 as a < b, a = b, a > b.
inline int compare_i64(std::int64_t a, std::int64_t b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

} // namespace nnh::collision::detail
