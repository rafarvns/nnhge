#pragma once
// The string pool of a map block: NUL-terminated strings, offset 0 = "", equal strings stored once.
// Pure (shared with tools/assetc).

#include <cstdint>

#include "tilemap/map_layout.hpp"

namespace nnh::detail {

/// True for a non-null string of at most mapfmt::kMaxStringLength bytes.
bool map_string_ok(const char* text);

/// The offset of `text` in the pool, adding it if new. -1 when the pool is full. `text` must be
/// map_string_ok(). "" is always offset 0 (needs no room).
int map_intern(const MapView& map, const char* text);

} // namespace nnh::detail
