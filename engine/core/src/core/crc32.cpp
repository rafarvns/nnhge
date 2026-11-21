// CRC-32 (see crc32.hpp). Pure: only the standard library, shared with the host tool nnh-assetc.

#include "core/crc32.hpp"

namespace nnh::detail {

std::uint32_t crc32(const void* data, std::size_t size) {
    // Bitwise and table-free: small, and no 1 KB table in every program that links it.
    std::uint32_t crc = 0xFFFFFFFFu;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

} // namespace nnh::detail
