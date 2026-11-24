#pragma once
// Wire primitives of structured saves (spec 027, D45): the wire byte, canonical varints, fixed widths and
// little-endian loads and stores. Pure and inline; tested in tests/saves/test_wire.cpp.
//
//   field  = wire:u8 id:varint value      wire bits 0-4: kind, bits 5-6: shape (0 One, 1 Array, 2 List), bit
//   7: 0 varint = unsigned LEB128, canonical (no needless continuation byte), at most 3 bytes (values < 2^21)

#include "nnh/saves.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "structured saves (spec 027) support little-endian systems only"
#endif

namespace nnh::detail {

inline constexpr std::uint32_t kSaveStructMagic = 0x56534E4Eu; // bytes "NNSV"
inline constexpr std::size_t kSaveStructHeaderSize = 16;       // before the key bytes
inline constexpr int kMaxVarintBytes = 3;
inline constexpr std::uint32_t kMaxVarint = (1u << 21) - 1;
inline constexpr int kWireKindCount = 20; // WireKind::None .. WireKind::Struct

constexpr std::uint8_t wire_byte(save::WireKind kind, save::WireShape shape) {
    return static_cast<std::uint8_t>(static_cast<unsigned>(kind) | (static_cast<unsigned>(shape) << 5));
}
constexpr save::WireKind wire_kind(std::uint8_t wire) {
    return static_cast<save::WireKind>(wire & 0x1F);
}
constexpr save::WireShape wire_shape(std::uint8_t wire) {
    return static_cast<save::WireShape>((wire >> 5) & 3);
}

/// A wire byte a file may hold: kind 1..19, shape 0..2, bit 7 clear.
constexpr bool wire_valid(std::uint8_t wire) {
    const unsigned kind = wire & 0x1Fu;
    const unsigned shape = (wire >> 5) & 3u;
    return (wire & 0x80u) == 0 && kind >= 1 && kind < static_cast<unsigned>(kWireKindCount) && shape <= 2;
}

/// Bytes of one value of a fixed-size kind; 0 for String and Struct (and None).
constexpr int wire_width(save::WireKind kind) {
    switch (kind) {
    case save::WireKind::Bool:
    case save::WireKind::U8:
    case save::WireKind::I8:
        return 1;
    case save::WireKind::U16:
    case save::WireKind::I16:
        return 2;
    case save::WireKind::U32:
    case save::WireKind::I32:
    case save::WireKind::F32:
    case save::WireKind::Color:
    case save::WireKind::DataRef:
    case save::WireKind::TextureRef:
    case save::WireKind::SoundRef:
    case save::WireKind::MusicRef:
        return 4;
    case save::WireKind::U64:
    case save::WireKind::I64:
    case save::WireKind::F64:
    case save::WireKind::Vec2:
        return 8;
    default:
        return 0;
    }
}

constexpr int varint_size(std::uint32_t value) {
    return value < 0x80u ? 1 : value < 0x4000u ? 2 : 3;
}

/// Writes `value` (< 2^21) at `out`; returns the bytes written (1-3). The caller checks the room.
inline int put_varint(std::uint8_t* out, std::uint32_t value) {
    if (value < 0x80u) {
        out[0] = static_cast<std::uint8_t>(value);
        return 1;
    }
    if (value < 0x4000u) {
        out[0] = static_cast<std::uint8_t>(value | 0x80u);
        out[1] = static_cast<std::uint8_t>(value >> 7);
        return 2;
    }
    out[0] = static_cast<std::uint8_t>(value | 0x80u);
    out[1] = static_cast<std::uint8_t>((value >> 7) | 0x80u);
    out[2] = static_cast<std::uint8_t>(value >> 14);
    return 3;
}

/// Reads a canonical varint of at most 3 bytes from [p, end). Returns the bytes read, or 0 when it is cut by
/// `end`, longer than 3 bytes, or not canonical (a last byte of 0 after a continuation).
inline int get_varint(const std::uint8_t* p, const std::uint8_t* end, std::uint32_t& value) {
    std::uint32_t v = 0;
    for (int i = 0; i < kMaxVarintBytes; ++i) {
        if (p + i >= end) return 0;
        const std::uint32_t b = p[i];
        v |= (b & 0x7Fu) << (7 * i);
        if ((b & 0x80u) == 0) {
            if (i > 0 && b == 0) return 0; // a needless continuation byte
            value = v;
            return i + 1;
        }
    }
    return 0; // a fourth byte would follow
}

/// Reads a varint from validated data (always well-formed). Advances `p`.
inline std::uint32_t take_varint(const std::uint8_t*& p) {
    std::uint32_t v = p[0] & 0x7Fu;
    if ((p[0] & 0x80u) == 0) {
        p += 1;
        return v;
    }
    v |= static_cast<std::uint32_t>(p[1] & 0x7Fu) << 7;
    if ((p[1] & 0x80u) == 0) {
        p += 2;
        return v;
    }
    v |= static_cast<std::uint32_t>(p[2]) << 14;
    p += 3;
    return v;
}

inline std::uint16_t load_u16(const std::uint8_t* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
inline std::uint32_t load_u32(const std::uint8_t* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
inline std::uint64_t load_u64(const std::uint8_t* p) {
    std::uint64_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
inline void store_u16(std::uint8_t* p, std::uint16_t v) {
    std::memcpy(p, &v, sizeof v);
}
inline void store_u32(std::uint8_t* p, std::uint32_t v) {
    std::memcpy(p, &v, sizeof v);
}

} // namespace nnh::detail
