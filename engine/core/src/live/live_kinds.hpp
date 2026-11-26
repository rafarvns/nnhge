#pragma once
// Live value kinds and their bits (spec 030 plan, "Kinds and bits"). A tunable's value is 32 bits on the wire and in
// the entry: float bits, int32, bool 0/1, colour r | g<<8 | b<<16 | a<<24, enum index; actions have none. Watches add
// 8/16-bit and unsigned integers, Vec2 (8 bytes) and text (<= 31 bytes). Pure, shared with tools/nnh.

#include <bit>
#include <cstdint>

namespace nnh::detail::live {

/// One enum for every kind (the `kind` byte of LiveEntry and LiveValue records).
enum class Kind : std::uint8_t {
    None = 0,
    Float = 1,
    Int32 = 2,
    Bool = 3,
    Color = 4,
    Enum = 5,
    Action = 6,
    Int8 = 7,
    Int16 = 8,
    UInt8 = 9,
    UInt16 = 10,
    UInt32 = 11,
    Vec2 = 12,
    Text = 13,
};
inline constexpr std::uint8_t kLastKind = static_cast<std::uint8_t>(Kind::Text);

/// Kinds a tunable can have (actions included: they're edited by running them).
constexpr bool tunable_kind(Kind k) { return k >= Kind::Float && k <= Kind::Action; }
/// Kinds a watch can have (no enums, no actions).
constexpr bool watch_kind(Kind k) { return k != Kind::None && k != Kind::Enum && k != Kind::Action && k <= Kind::Text; }

/// Bytes of a value of `k` in a LiveValue record (text: up to kMaxTextBytes, its own length).
constexpr int value_bytes(Kind k) {
    switch (k) {
    case Kind::Vec2: return 8;
    case Kind::Action:
    case Kind::None:
    case Kind::Text: return 0;
    default: return 4;
    }
}

inline constexpr int kMaxTextBytes = 31;

constexpr std::uint32_t float_bits(float v) { return std::bit_cast<std::uint32_t>(v); }
constexpr float bits_float(std::uint32_t b) { return std::bit_cast<float>(b); }

constexpr std::uint32_t pack_rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(a) << 24);
}
/// Channel 0 R, 1 G, 2 B, 3 A of packed colour bits.
constexpr std::uint8_t channel(std::uint32_t rgba, int c) { return static_cast<std::uint8_t>(rgba >> (8 * c)); }
constexpr std::uint32_t with_channel(std::uint32_t rgba, int c, std::uint8_t v) {
    return (rgba & ~(0xFFu << (8 * c))) | (static_cast<std::uint32_t>(v) << (8 * c));
}

} // namespace nnh::detail::live
