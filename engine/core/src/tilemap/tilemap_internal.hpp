#pragma once
// Shared by the tilemap files (tilemap.cpp, map_build.cpp, map_tiles.cpp, ...). Internal.

#include <cstddef>
#include <cstdint>

#include "nnh/memory.hpp"
#include "nnh/result.hpp"
#include "nnh/tilemap.hpp"
#include "tilemap/map_layout.hpp"

namespace nnh {

/// Reaches into nnh::Tilemap and nnh::Properties for the engine (friend of both).
struct TilemapAccess {
    static std::uint8_t* block(const Tilemap& map) { return map.block_; }
    /// A Tilemap owning `block` (allocated with the `tilemap` tag, a valid map block).
    static Tilemap adopt(std::uint8_t* block) {
        Tilemap map;
        map.block_ = block;
        return map;
    }
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

/// Validates a `.nnhmap` file and copies it into a new map with `options`' extra room (no textures yet: tilesets
/// have no texture). `name` is for messages. Errors as load_tilemap's for the file. Load time only.
Result<Tilemap> tilemap_from_file(const void* data, std::size_t size, const LoadTilemapOptions& options,
                                  const char* name);

/// Reads the `collision_layers` int properties of a loaded map's tile layers and tiles into its runtime sets
/// (spec 019 FR-6); a non-int or 0 value is ignored with one warning per map naming `name` (map_load_layers.cpp).
void load_collision_layers(Tilemap& map, const char* name);

/// Frees the tileset textures load_tilemap loaded into the block (Tilemap::reset calls it).
void release_map_textures(std::uint8_t* block);

/// For the Result-returning edits: an error if the map is empty.
inline bool map_empty(const Tilemap& map) { return TilemapAccess::block(map) == nullptr; }

} // namespace detail
} // namespace nnh
