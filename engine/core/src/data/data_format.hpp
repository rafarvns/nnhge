#pragma once
// Data assets' converted format `NNHD` (version 1) and the schema limits (spec 014 FR-10, FR-14; plan 014
// "Converted file format"). Pure: shared by core, the schema host program and nnh-assetc.
//
// A converted data asset, little-endian, no alignment (read with memcpy), at most 64 KiB:
//   header (20 B): magic "NNHD", u16 version, u16 flags (0), u32 file size, u32 CRC-32 of the body,
//                  u32 schema fingerprint of the asset's type;
//   body: the root struct value.
// A struct value is `u8 n` then n pairs `(u8 field index, value)`, indices strictly increasing (unset fields
// are absent and keep their defaults). Values by kind: bool u8 0/1; int in its declared width (1, 2 or 4
// bytes); float u32 IEEE bits (finite); string u8 length + bytes (<= capacity, no NUL, valid UTF-8); enum u8
// name index; vec2 two floats; color 4 bytes RGBA; every reference u32 hash (0 = none); struct a struct
// value; array N values; list u16 count (<= N) + count values.

#include <cstddef>
#include <cstdint>

namespace nnh::detail::datafmt {

inline constexpr char kMagic[4] = {'N', 'N', 'H', 'D'};
inline constexpr std::uint16_t kVersion = 1;

// Header field offsets.
inline constexpr std::size_t kVersionAt = 4;
inline constexpr std::size_t kFlagsAt = 6;
inline constexpr std::size_t kSizeAt = 8;
inline constexpr std::size_t kCrcAt = 12;
inline constexpr std::size_t kFingerprintAt = 16;
inline constexpr std::size_t kHeaderSize = 20;

inline constexpr std::size_t kMaxFileBytes = 64 * 1024; ///< a converted data asset

// Schema limits (FR-14): checked by check_schema (the host program) and by the description reader.
inline constexpr int kMaxDataTypes = 64; ///< types with a key
inline constexpr int kMaxTypes = 128;    ///< types and nested-only structs
inline constexpr int kMaxEnums = 64;
inline constexpr int kMaxEnumNames = 255;
inline constexpr int kMaxNameLength = 63; ///< type, field and enum names, enum value names
inline constexpr int kMaxKeyLength = 16;

} // namespace nnh::detail::datafmt
