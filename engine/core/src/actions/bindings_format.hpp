#pragma once
// The saved bindings blob `NNHB` (spec 017 FR-20, FR-21; decision D32). Little-endian:
//   0  magic "NNHB"   4  format version (1)   5  reserved (0)   6  entry count u16 (<= 256)
//   8  game bindings version u32   12  CRC-32 of the entries   16  entries, 12 bytes each:
//   action name hash u32 · slot u8 · part u8 (nnh::Part) · source kind u8 (nnh::Source::Kind) · flags u8 (bit 0
//   invert x, bit 1 invert y) · data[4] (Source::a..d; a part entry: data[0] = the button, kind Button or None).
// Pure: encoding, structural validation and decoding; what an entry means is checked when applied
// (bindings_io.cpp). Shared with the fuzz target.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::bindings {

inline constexpr std::uint8_t kFormat = 1;
inline constexpr std::size_t kHeaderSize = 16;
inline constexpr std::size_t kEntrySize = 12;
inline constexpr int kMaxEntries = 256;
inline constexpr std::uint8_t kInvertX = 1, kInvertY = 2;

struct Entry {
    std::uint32_t hash = 0;
    std::uint8_t slot = 0;
    std::uint8_t part = 0;
    std::uint8_t kind = 0;
    std::uint8_t flags = 0;
    std::uint8_t data[4] = {};
};

/// Writes one entry's 12 bytes at `out`.
void encode_entry(const Entry& e, std::uint8_t* out);
/// Reads the 12 bytes at `in`.
Entry decode_entry(const std::uint8_t* in);
/// Fills the 16-byte header in front of `count` entries already at `blob + kHeaderSize` (computes their CRC).
void finish_blob(std::uint8_t* blob, int count, std::uint32_t game_version);
/// Sorts the `count` entries at `entries` by (hash, slot, part), in place.
void sort_entries(std::uint8_t* entries, int count);

enum class Check : std::uint8_t { Ok, Corrupted, Unsupported };

/// Structural checks of a whole blob: size, magic, format (newer: Unsupported), reserved byte, count, CRC.
/// On Ok, `count` and `game_version` are set.
Check validate(const void* data, std::size_t size, int& count, std::uint32_t& game_version);

} // namespace nnh::detail::bindings
