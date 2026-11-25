#pragma once
// Bulk byte work of structured saves (spec 027 T10.3): packed copies, single values and bool arrays moved a
// 32-bit word at a time. On the 3DS, newlib's memcpy copies byte by byte whenever source and destination are
// not equally aligned (a packed sequence inside a file usually isn't), and each call of a few bytes costs
// more than the copy; these inline loops use unaligned word loads and stores (allowed on the ARM11 and on
// x86) and give the same bytes. Pure; tested in tests/saves/test_bulk.cpp.

#include "saves/save_wire.hpp"

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

/// Copies `n` bytes from `src` to `dst` (not overlapping), two words at a time.
inline void copy_bytes(std::uint8_t* dst, const std::uint8_t* src, std::size_t n) {
    for (; n >= 8; n -= 8, src += 8, dst += 8) {
        const std::uint32_t a = load_u32(src);
        const std::uint32_t b = load_u32(src + 4);
        store_u32(dst, a);
        store_u32(dst + 4, b);
    }
    if (n >= 4) {
        store_u32(dst, load_u32(src));
        n -= 4;
        src += 4;
        dst += 4;
    }
    for (; n > 0; --n)
        *dst++ = *src++;
}

/// Copies one value of `width` bytes: 1, 2, 4 or 8.
inline void copy_value(std::uint8_t* dst, const std::uint8_t* src, int width) {
    switch (width) {
    case 1:
        *dst = *src;
        break;
    case 2:
        store_u16(dst, load_u16(src));
        break;
    case 4:
        store_u32(dst, load_u32(src));
        break;
    default: {
        const std::uint32_t a = load_u32(src);
        const std::uint32_t b = load_u32(src + 4);
        store_u32(dst, a);
        store_u32(dst + 4, b);
        break;
    }
    }
}

/// The little-endian value of the `width` bytes (1, 2, 4 or 8) at `src`.
inline std::uint64_t load_value(const std::uint8_t* src, int width) {
    switch (width) {
    case 1:
        return *src;
    case 2:
        return load_u16(src);
    case 4:
        return load_u32(src);
    default:
        return load_u64(src);
    }
}

/// Stores the low `width` bytes (1, 2, 4 or 8) of `value` at `dst`, little-endian.
inline void store_value(std::uint8_t* dst, std::uint64_t value, int width) {
    switch (width) {
    case 1:
        *dst = static_cast<std::uint8_t>(value);
        break;
    case 2:
        store_u16(dst, static_cast<std::uint16_t>(value));
        break;
    case 4:
        store_u32(dst, static_cast<std::uint32_t>(value));
        break;
    default:
        store_u32(dst, static_cast<std::uint32_t>(value));
        store_u32(dst + 4, static_cast<std::uint32_t>(value >> 32));
        break;
    }
}

/// Writes the `n` bool bytes at `src` as 0 or 1 at `dst` (any non-zero byte is true).
inline void put_bools(std::uint8_t* dst, const std::uint8_t* src, std::size_t n) {
    for (; n >= 4; n -= 4, src += 4, dst += 4) {
        const std::uint32_t w = load_u32(src);
        // Per byte: bit 7 set when the low seven bits carry into it or it was set: the byte is not zero.
        store_u32(dst, ((((w & 0x7F7F7F7Fu) + 0x7F7F7F7Fu) | w) >> 7) & 0x01010101u);
    }
    for (; n > 0; --n)
        *dst++ = *src++ != 0 ? 1 : 0;
}

/// Copies the `n` bool bytes at `src` while each is 0 or 1. Returns `n`, or the index of the first byte above
/// 1 (the bytes before it are copied; it and the bytes after it are not written).
inline std::size_t copy_bools(std::uint8_t* dst, const std::uint8_t* src, std::size_t n) {
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        const std::uint32_t w = load_u32(src + i);
        if ((w & 0xFEFEFEFEu) != 0) break;
        store_u32(dst + i, w);
    }
    for (; i < n; ++i) {
        if (src[i] > 1) return i;
        dst[i] = src[i];
    }
    return n;
}

} // namespace nnh::detail
