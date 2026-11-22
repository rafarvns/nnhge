// CRC-32 continued piece by piece (see crc32.hpp). Its own file so programs that only call crc32() don't link it
// (PC links whole objects from the static library). Pure: shared with the host tools.

#include "core/crc32.hpp"

namespace nnh::detail {

std::uint32_t crc32_update(std::uint32_t crc, const void* data, std::size_t size) {
    // The loop of crc32(), kept apart so programs that only call crc32() link no more code.
    crc = ~crc;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

} // namespace nnh::detail
