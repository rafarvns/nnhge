// Value conversions of structured saves (save_convert.hpp). Integer arithmetic only: floats move as their
// bits.

#include "saves/save_convert.hpp"

#include "saves/save_bulk.hpp"
#include "saves/save_wire.hpp"

#include <bit>
#include <cstring>

namespace nnh::detail {
namespace {

using save::WireKind;

/// An integer as a sign and a magnitude (every 64-bit value, signed or not, fits).
struct Int {
    bool negative = false;
    std::uint64_t magnitude = 0;
};

bool is_integer(WireKind k) {
    return k >= WireKind::U8 && k <= WireKind::I64;
}
bool is_signed(WireKind k) {
    return k == WireKind::I8 || k == WireKind::I16 || k == WireKind::I32 || k == WireKind::I64;
}

Int read_int(WireKind kind, const std::uint8_t* src) {
    const int width = wire_width(kind);
    std::uint64_t raw = load_value(src, width); // little-endian: the low bytes
    Int v;
    if (is_signed(kind) && width < 8 && ((raw >> (8 * width - 1)) & 1u) != 0) {
        raw |= ~0ull << (8 * width); // sign extension
    }
    if (is_signed(kind) && (raw >> 63) != 0) {
        v.negative = true;
        v.magnitude = 0 - raw;
    } else {
        v.magnitude = raw;
    }
    return v;
}

bool fits(const Int& v, WireKind target) {
    const int bits = 8 * wire_width(target);
    if (!is_signed(target)) return !v.negative && (bits == 64 || v.magnitude >> bits == 0);
    const std::uint64_t limit = 1ull << (bits - 1); // -limit .. limit - 1
    return v.negative ? v.magnitude <= limit : v.magnitude < limit;
}

bool enum_allows(const EnumInfo* names, const Int& v) {
    if (names == nullptr) return true;
    const std::int64_t value =
        v.negative ? static_cast<std::int64_t>(0 - v.magnitude) : static_cast<std::int64_t>(v.magnitude);
    for (int i = 0; i < names->count; ++i) {
        if (names->values[i] == value) return true;
    }
    return false;
}

/// Trailing zero bits of a non-zero 64-bit value, in 32-bit halves (no libgcc call on the 3DS's 32-bit ARM).
int trailing_zeros(std::uint64_t v) {
    const auto low = static_cast<std::uint32_t>(v);
    return low != 0 ? std::countr_zero(low) : 32 + std::countr_zero(static_cast<std::uint32_t>(v >> 32));
}

int bit_length(std::uint64_t v) {
    const auto high = static_cast<std::uint32_t>(v >> 32);
    return high != 0 ? 64 - std::countl_zero(high) : 32 - std::countl_zero(static_cast<std::uint32_t>(v));
}

/// The IEEE bits of the integer, when exactly representable with `mantissa` stored bits.
bool int_to_float_bits(const Int& v, int mantissa, int bias, std::uint64_t& bits) {
    if (v.magnitude == 0) {
        bits = 0;
        return true;
    }
    const int length = bit_length(v.magnitude);
    const int significant = length - trailing_zeros(v.magnitude);
    if (significant > mantissa + 1) return false;
    const std::uint64_t normalized = v.magnitude << (64 - length); // the top bit at 63
    const std::uint64_t fraction = (normalized << 1) >> (64 - mantissa);
    const std::uint64_t exponent = static_cast<std::uint64_t>(length - 1 + bias);
    const int sign_bit = mantissa == 23 ? 31 : 63;
    bits = (static_cast<std::uint64_t>(v.negative) << sign_bit) | (exponent << mantissa) | fraction;
    return true;
}

std::uint64_t f32_to_f64(std::uint32_t f) {
    const std::uint64_t sign = static_cast<std::uint64_t>(f >> 31) << 63;
    const std::uint32_t exponent = (f >> 23) & 0xFFu;
    std::uint64_t fraction = f & 0x7FFFFFu;
    if (exponent == 0) {
        if (fraction == 0) return sign;
        const int top = bit_length(fraction) - 1; // a subnormal: normalise (it is normal as a double)
        fraction = (fraction << (52 - top)) & 0xFFFFFFFFFFFFFull;
        return sign | (static_cast<std::uint64_t>(top - 149 + 1023) << 52) | fraction;
    }
    return sign | (static_cast<std::uint64_t>(exponent - 127 + 1023) << 52) | (fraction << 29);
}

bool f64_to_f32(std::uint64_t d, std::uint32_t& out) {
    const std::uint32_t sign = static_cast<std::uint32_t>(d >> 63) << 31;
    const int exponent = static_cast<int>((d >> 52) & 0x7FFu);
    const std::uint64_t fraction = d & 0xFFFFFFFFFFFFFull;
    if (exponent == 0) { // zero (a double subnormal is far below a float's range)
        if (fraction != 0) return false;
        out = sign;
        return true;
    }
    const int e = exponent - 1023;
    if (e > 127 || e < -149) return false;
    if (e >= -126) {
        if ((fraction & 0x1FFFFFFFull) != 0) return false;
        out = sign | (static_cast<std::uint32_t>(e + 127) << 23) | static_cast<std::uint32_t>(fraction >> 29);
        return true;
    }
    const int shift = -97 - e; // a float subnormal: (2^52 + fraction) >> shift, exact when no bit is lost
    const std::uint64_t full = (1ull << 52) | fraction;
    if ((full & ((1ull << shift) - 1)) != 0) return false;
    out = sign | static_cast<std::uint32_t>(full >> shift);
    return true;
}

} // namespace

bool convert_scalar(WireKind from, const std::uint8_t* src, const SaveFieldInfo& f, const EnumInfo* names,
                    std::uint8_t* dst) {
    const WireKind to = f.kind;
    if (is_integer(from)) {
        const Int v = read_int(from, src);
        if (is_integer(to)) {
            if (!fits(v, to) || !enum_allows(names, v)) return false;
            store_value(dst, v.negative ? 0 - v.magnitude : v.magnitude, wire_width(to));
            return true;
        }
        std::uint64_t bits = 0;
        if (to == WireKind::F32 && int_to_float_bits(v, 23, 127, bits)) {
            store_u32(dst, static_cast<std::uint32_t>(bits));
            return true;
        }
        if (to == WireKind::F64 && int_to_float_bits(v, 52, 1023, bits)) {
            std::memcpy(dst, &bits, 8);
            return true;
        }
        return false;
    }
    switch (from) {
    case WireKind::Bool:
        if (to != WireKind::Bool || *src > 1) return false;
        *dst = *src;
        return true;
    case WireKind::F32: {
        const std::uint32_t bits = load_u32(src);
        if (!f32_finite(bits)) return false;
        if (to == WireKind::F32) {
            store_u32(dst, bits);
            return true;
        }
        if (to != WireKind::F64) return false;
        const std::uint64_t wide = f32_to_f64(bits);
        std::memcpy(dst, &wide, 8);
        return true;
    }
    case WireKind::F64: {
        const std::uint64_t bits = load_u64(src);
        if (!f64_finite(bits)) return false;
        if (to == WireKind::F64) {
            std::memcpy(dst, &bits, 8);
            return true;
        }
        std::uint32_t narrow = 0;
        if (to != WireKind::F32 || !f64_to_f32(bits, narrow)) return false;
        store_u32(dst, narrow);
        return true;
    }
    case WireKind::Vec2:
        if (to != WireKind::Vec2 || !f32_finite(load_u32(src)) || !f32_finite(load_u32(src + 4)))
            return false;
        std::memcpy(dst, src, 8);
        return true;
    default: // Color and the references: the same kind only
        if (to != from) return false;
        copy_value(dst, src, wire_width(from));
        return true;
    }
}

bool convert_string(const std::uint8_t* src, int capacity, std::uint8_t* dst) {
    int n = src[0];
    const std::uint8_t* text = src + 1;
    const bool cut = n > capacity;
    if (cut) {
        n = capacity; // back over continuation bytes to the lead byte of the character that doesn't fit
        while (n > 0 && (text[n] & 0xC0u) == 0x80u)
            --n;
    }
    std::memcpy(dst, text, static_cast<std::size_t>(n));
    std::memset(dst + n, 0, static_cast<std::size_t>(capacity + 1 - n));
    return cut;
}

} // namespace nnh::detail
