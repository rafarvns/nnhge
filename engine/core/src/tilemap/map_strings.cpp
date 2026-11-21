// The string pool of a map block (see map_strings.hpp).

#include "tilemap/map_strings.hpp"

#include <cstring>

namespace nnh::detail {

bool map_string_ok(const char* text) {
    if (text == nullptr) return false;
    for (int i = 0; i <= mapfmt::kMaxStringLength; ++i) {
        if (text[i] == '\0') return true;
    }
    return false;
}

int map_intern(const MapView& map, const char* text) {
    if (text[0] == '\0') return 0;
    mapfmt::Header& h = map.header();
    char* pool = map.strings();
    for (std::uint32_t at = 1; at < h.string_bytes; at += static_cast<std::uint32_t>(std::strlen(pool + at)) + 1) {
        if (std::strcmp(pool + at, text) == 0) return static_cast<int>(at);
    }
    const std::uint32_t size = static_cast<std::uint32_t>(std::strlen(text)) + 1;
    // Offsets are u16: a string must also start below 65536 (the pool is at most 64 KiB anyway).
    if (h.string_bytes + size > h.string_cap) return -1;
    const std::uint32_t at = h.string_bytes;
    std::memcpy(pool + at, text, size);
    h.string_bytes += size;
    return static_cast<int>(at);
}

} // namespace nnh::detail
