#pragma once
// Shared by the tilemap files (tilemap.cpp, map_build.cpp, map_tiles.cpp, ...). Internal.

#include <cstdint>

#include "nnh/memory.hpp"
#include "nnh/result.hpp"
#include "nnh/tilemap.hpp"
#include "tilemap/map_layout.hpp"

namespace nnh {

/// Reaches into nnh::Tilemap and nnh::Properties for the engine (friend of both).
struct TilemapAccess {
    static std::uint8_t* block(const Tilemap& map) { return map.block_; }
    static Properties properties(const std::uint8_t* block, int first, int count) {
        Properties p;
        p.block_ = block;
        p.first_ = first;
        p.count_ = count;
        return p;
    }
    static const std::uint8_t* block(const Properties& p) { return p.block_; }
    static int first(const Properties& p) { return p.first_; }
};

namespace detail {

MemTag& tilemap_memory_tag();

/// The section capacities create() uses for `desc` (tile properties: up to min(properties, tile ids)).
MapCapacities capacities_for(const TilemapDesc& desc);

/// The map's block as a view (base null for an empty map).
inline MapView map_view(const Tilemap& map) { return {TilemapAccess::block(map)}; }

/// Logs a warning on the `tilemap` channel and makes the error ("tilemap: <what>").
Error map_refuse(ErrorCode code, const char* what);

/// True while the texture of tileset `tileset` is loaded (false once it was destroyed, even if its slot was
/// reused, and for an invalid tileset). Drawing skips tilesets whose texture is gone.
bool tileset_texture_alive(const Tilemap& map, int tileset);

/// For the Result-returning edits: an error if the map is empty.
inline bool map_empty(const Tilemap& map) { return TilemapAccess::block(map) == nullptr; }

} // namespace detail
} // namespace nnh
