#pragma once
// Internals of the 3D math (spec 026, D44): exponent tests on float bits (no float compare decides a degenerate case,
// so flush-to-zero on the 3DS never changes a result), the correctly rounded square root, the eighth-turn snap and the
// look basis shared by look_rotation and look_at. Included only by src/math3d/*.cpp (-ffp-contract=off).

#include <bit>
#include <cstdint>

#include "nnh/math3d.hpp"

namespace nnh::detail {

/// The exponent field of a float's bits.
inline std::uint32_t exponent_field(float f) { return (std::bit_cast<std::uint32_t>(f) >> 23) & 0xFFu; }
/// |f| < 2⁻¹⁰⁰ (zero and subnormals included): the engine's "zero" for norms, lengths and determinants.
inline bool tiny(float f) { return exponent_field(f) < 127u - 100u; }
/// Neither infinite nor NaN.
inline bool finite(float f) { return exponent_field(f) != 0xFFu; }
/// Usable as a divisor or under a root: finite and not tiny.
inline bool usable(float f) { return finite(f) && !tiny(f); }
/// Negative by the sign bit (−0 included).
inline bool sign_bit(float f) { return (std::bit_cast<std::uint32_t>(f) & 0x80000000u) != 0; }

/// The IEEE square root: one correctly rounded instruction with -fno-math-errno (x86-64 sqrtss, VFP vsqrt).
inline float sqrt_ieee(float f) { return __builtin_sqrtf(f); }

/// When |sin| and |cos| are within 2⁻²⁰ of each other (an odd multiple of an eighth turn), makes both magnitudes the
/// same bits (the larger), keeping their signs: quarter-turn quaternions and 90° projections become exact.
SinCos snap_eighth(SinCos sc);

/// The camera basis looking along `forward` with `up`: z = −forward, x = up × z, y = z × x, all unit. A forward parallel
/// to `up` uses the up (0, 0, ∓1) (north at the top when looking down). @return false when `forward` is zero.
bool look_basis(Vec3 forward, Vec3 up, Vec3& x, Vec3& y, Vec3& z);

} // namespace nnh::detail
