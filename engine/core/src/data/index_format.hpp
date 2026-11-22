#pragma once
// The data index `.nnh_data` (version 1; spec 014 FR-8, FR-12, FR-13; plan 014 "Data index"): every data
// asset of the game by type and path, and every media asset a data asset references, so references (32-bit
// path hashes) turn back into paths and assets can be listed by type. Written by `nnh-assetc data-index`,
// read by core. Pure: shared by core and nnh-assetc.
//
// Little-endian, no alignment (read byte by byte):
//   header (32 B): magic "NNHX", u16 version, u16 flags (0), u32 file size, u32 CRC-32 of everything after
//   the
//                  header, u16 type count, u16 entry counts of the groups data / texture / sound / music, u16
//                  0, u32 size of the largest data asset (the loader's scratch buffer);
//   types (28 B each): key (16 B, NUL-padded), u32 schema fingerprint, u32 first entry, u32 entry count —
//   every
//                  keyed type of the build's schema, even one without assets; their entries are consecutive;
//   entries (8 B each): u32 path hash (FNV-1a 32 of the path without extension), u32 offset of the path in
//   the
//                  strings; data entries first (by type, then by path), then textures, sounds and music (by
//                  path);
//   lookup (2 B each): every entry index once, sorted by (group, hash), for binary search;
//   strings: NUL-terminated paths without extension, up to the end of the file.

#include <cstddef>
#include <cstdint>

#include "data/data_format.hpp"

namespace nnh::detail::idxfmt {

inline constexpr const char* kAssetName = ".nnh_data"; ///< reserved asset name (like .nnh_assets)

inline constexpr char kMagic[4] = {'N', 'N', 'H', 'X'};
inline constexpr std::uint16_t kVersion = 1;

// Header field offsets.
inline constexpr std::size_t kVersionAt = 4;
inline constexpr std::size_t kFlagsAt = 6;
inline constexpr std::size_t kSizeAt = 8;
inline constexpr std::size_t kCrcAt = 12;
inline constexpr std::size_t kTypeCountAt = 16;
inline constexpr std::size_t kGroupCountsAt = 18; ///< 4 x u16
inline constexpr std::size_t kReservedAt = 26;
inline constexpr std::size_t kLargestAt = 28;
inline constexpr std::size_t kHeaderSize = 32;

inline constexpr std::size_t kTypeSize = 28;
inline constexpr std::size_t kKeyBytes = 16; ///< datafmt::kMaxKeyLength
inline constexpr std::size_t kEntrySize = 8;
inline constexpr std::size_t kLookupSize = 2;

/// Entry groups, in file order. A reference's kind selects its group.
enum class Group : std::uint8_t { Data, Texture, Sound, Music };
inline constexpr int kGroups = 4;

// Limits (FR-14).
inline constexpr int kMaxTypes = datafmt::kMaxDataTypes;
inline constexpr int kMaxDataEntries = 4096;
inline constexpr int kMaxMediaEntries = 4096; ///< textures + sounds + music
inline constexpr std::size_t kMaxStringBytes = 256 * 1024;
inline constexpr std::size_t kMaxFileBytes =
    kHeaderSize + kTypeSize * kMaxTypes + (kEntrySize + kLookupSize) * (kMaxDataEntries + kMaxMediaEntries) +
    kMaxStringBytes;

} // namespace nnh::detail::idxfmt
