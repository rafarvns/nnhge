#pragma once
/// @file nnh/detail/live_kind_of.hpp
/// The kind byte of a watched type (spec 030; src/live/live_kinds.hpp's Kind), with compiler builtins (`__is_same`)
/// instead of <type_traits>. Engine-internal; include <nnh/live.hpp>.

#include <cstdint>

#include "nnh/math.hpp"

namespace nnh {
struct Color;
} // namespace nnh

namespace nnh::detail::live_api {

/// A plain integer type of `T` (not bool, not char, which reads as text).
template <typename T>
constexpr bool is_integer() {
    return __is_same(T, signed char) || __is_same(T, unsigned char) || __is_same(T, short) ||
           __is_same(T, unsigned short) || __is_same(T, int) || __is_same(T, unsigned int) || __is_same(T, long) ||
           __is_same(T, unsigned long) || __is_same(T, long long) || __is_same(T, unsigned long long);
}
/// A 32-bit signed integer type: `int`, and `long` where it has 32 bits (std::int32_t is `long` on the 3DS).
template <typename T>
constexpr bool is_int32() {
    return (__is_same(T, int) || __is_same(T, long)) && sizeof(T) == 4;
}

/// The kind byte of a watched type (src/live/live_kinds.hpp's Kind), by size and signedness for integers so `int`,
/// `long` and the <cstdint> types work the same on every system; 0 for unsupported types.
template <typename T>
constexpr std::uint8_t kind_of() {
    if constexpr (__is_same(T, bool)) return 3;
    else if constexpr (__is_same(T, float)) return 1;
    else if constexpr (__is_same(T, nnh::Vec2)) return 12;
    else if constexpr (__is_same(T, nnh::Color)) return 4;
    else if constexpr (is_integer<T>() && sizeof(T) <= 4) {
        constexpr bool sign = static_cast<T>(-1) < static_cast<T>(0);
        if constexpr (sizeof(T) == 1) return sign ? 7 : 9;
        else if constexpr (sizeof(T) == 2) return sign ? 8 : 10;
        else return sign ? 2 : 11;
    } else return 0;
}

} // namespace nnh::detail::live_api
