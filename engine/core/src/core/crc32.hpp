#pragma once
// CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320, init and final XOR 0xFFFFFFFF), as zlib's crc32().
// Pure: no engine includes, so the host tool nnh-assetc (tools/assetc) compiles this file too. Used by save
// files and .nnhmap maps. Tests: tests/core/test_crc32.cpp.

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

/// Checksum of `size` bytes at `data` (`data` may be null when `size` is 0; the result is then 0).
/// Never allocates; bitwise, table-free (about 8 shifts per byte: fine for files, not for hot loops).
std::uint32_t crc32(const void* data, std::size_t size);

} // namespace nnh::detail
