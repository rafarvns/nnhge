#pragma once
// Integer tests on float bits for the particle loops (spec 024 plan "ARM11 cost plan" 3): no float compare, no vmrs.

#include <bit>
#include <cstdint>

namespace nnh::detail {

/// A float's order as a signed integer: a < b ⇔ key(a) < key(b) for every non-NaN pair (−0 just below +0).
inline std::int32_t order_key(float f) {
    const auto b = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(f));
    return b ^ ((b >> 31) & 0x7FFFFFFF);
}

/// The float of an order key.
inline float key_float(std::int32_t k) {
    const std::int32_t b = k ^ ((k >> 31) & 0x7FFFFFFF);
    return std::bit_cast<float>(static_cast<std::uint32_t>(b));
}

/// +0 for a value whose magnitude is below 2⁻⁶⁰ (exponent field < 67): no value reaches the subnormal range, where the
/// 3DS (flush-to-zero) and the PC would differ (plan decision 11).
inline float flush_tiny(float v) {
    const std::uint32_t b = std::bit_cast<std::uint32_t>(v);
    return ((b >> 23) & 0xFFu) < 67u ? 0.0f : v;
}

/// True for a finite float (exponent field not 255).
inline bool finite_float(float v) { return (std::bit_cast<std::uint32_t>(v) & 0x7F800000u) != 0x7F800000u; }

} // namespace nnh::detail
