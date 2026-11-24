#pragma once
// Save file format: 16-byte little-endian header + payload. Pure functions; unit tested in
// tests/fs/test_save.cpp and fuzzed through tests/fuzz.
//
//   offset 0  u32 magic 'NNHS'   4  u16 version   6  u16 flags (0)   8  u32 payload size   12  u32 crc32

#include <cstddef>
#include <cstdint>

#include "core/crc32.hpp" // the payload checksum, detail::crc32()
#include "nnh/result.hpp"

namespace nnh::detail {

inline constexpr std::size_t kSaveHeaderSize = 16;
inline constexpr std::uint32_t kSaveMagic = 0x53484E4Eu; // bytes "NNHS"
inline constexpr std::uint32_t kMaxSavePayload = 16u * 1024 * 1024;

/// Writes the header into `out` (kSaveHeaderSize bytes). @return kSaveHeaderSize.
std::size_t build_save_header(std::uint8_t* out, std::uint32_t payload_size, std::uint16_t version,
                              std::uint32_t crc);

struct SaveHeader {
    std::uint16_t version;
    std::uint32_t size;
    std::uint32_t crc;
};

/// Parses and checks a header (magic, size limit). Does not check the payload.
Result<SaveHeader> parse_save_header(const std::uint8_t* data, std::size_t size);

struct SaveView {
    const std::uint8_t* payload;
    std::size_t size;
    std::uint16_t version;
};

/// Validates a complete save file in memory: header, exact length, CRC. The view points into `data`.
Result<SaveView> validate_save(const std::uint8_t* data, std::size_t size);

/// Reads and checks one save file exactly as nnh::save::read does for the main file or its backup (no fallback):
/// header, length, capacity (LimitReached), CRC. NotFound, Corrupted, LimitReached, IoError. For spec 027's
/// structured saves, which choose the backup themselves. Defined at the end of save.cpp.
Result<std::size_t> save_read_one(const char* path, void* buffer, std::size_t capacity, std::uint16_t* version_out);

} // namespace nnh::detail
