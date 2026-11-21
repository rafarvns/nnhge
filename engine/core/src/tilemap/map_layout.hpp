#pragma once
// Map capacities → section offsets and the block size (the plan's memory formula), limit checks, and a
// typed view over a map block. Pure (shared with tools/assetc); unit tested in tests/tilemap/test_map_layout.cpp.

#include <cstdint>

#include "tilemap/map_format.hpp"

namespace nnh::detail {

/// How many entries each section has room for. Negative values are invalid.
struct MapCapacities {
    int width = 0, height = 0; ///< cells per layer
    int tilesets = 0;
    int tile_ids = 0;    ///< the tile table has one more entry: id 0 (empty)
    int tile_props = 0;  ///< tiles that can have properties
    int animations = 0, animation_frames = 0;
    int tile_layers = 0, object_layers = 0, objects = 0, properties = 0;
    int string_bytes = 0; ///< the whole pool, its empty string included
};

struct MapLayout {
    std::uint32_t offsets[mapfmt::kSectionCount] = {};
    std::uint32_t sizes[mapfmt::kSectionCount] = {};
    std::uint32_t total = 0; ///< bytes of the block
};

/// Lays out the sections after the header, in mapfmt::Section order, each 4-byte aligned.
/// Capacities must have passed check_capacities().
MapLayout map_layout(const MapCapacities& capacities);

struct CapacityCheck {
    bool ok = true;
    bool limit = false;           ///< true: over an engine limit (LimitReached); false: invalid value
    const char* reason = nullptr; ///< static text when !ok
};

/// Checks capacities against the format's limits (map 1..1024 per side, the per-section maxima, and at
/// most 4 Mi cells over all tile layers).
CapacityCheck check_capacities(const MapCapacities& capacities);

/// The capacities a block's header declares.
MapCapacities capacities_of(const mapfmt::Header& header);

/// Typed access to the sections of a map block (no checks: the block is trusted).
struct MapView {
    std::uint8_t* base = nullptr;

    mapfmt::Header& header() const { return *reinterpret_cast<mapfmt::Header*>(base); }
    template <typename T>
    T* section(mapfmt::Section s) const {
        return reinterpret_cast<T*>(base + header().offsets[s]);
    }
    mapfmt::TilesetEntry* tilesets() const { return section<mapfmt::TilesetEntry>(mapfmt::kTilesets); }
    mapfmt::TileSource* tile_sources() const { return section<mapfmt::TileSource>(mapfmt::kTileSources); }
    std::uint8_t* tile_flags() const { return section<std::uint8_t>(mapfmt::kTileFlags); }
    mapfmt::TilePropsEntry* tile_props() const { return section<mapfmt::TilePropsEntry>(mapfmt::kTileProps); }
    mapfmt::TileLayerEntry* tile_layers() const { return section<mapfmt::TileLayerEntry>(mapfmt::kTileLayers); }
    mapfmt::ObjectLayerEntry* object_layers() const {
        return section<mapfmt::ObjectLayerEntry>(mapfmt::kObjectLayers);
    }
    mapfmt::ObjectEntry* objects() const { return section<mapfmt::ObjectEntry>(mapfmt::kObjects); }
    mapfmt::PropertyEntry* properties() const { return section<mapfmt::PropertyEntry>(mapfmt::kProperties); }
    char* strings() const { return section<char>(mapfmt::kStrings); }
    std::uint16_t* cells(int layer) const {
        return reinterpret_cast<std::uint16_t*>(base + tile_layers()[layer].cells);
    }
    const char* string(std::uint16_t offset) const { return strings() + offset; }
};

/// Writes a fresh header (magic, version, sizes, capacities, offsets; counts 0) and the empty string into a
/// zeroed block of map_layout(capacities).total bytes, and points each layer entry at its cells.
void init_map_block(std::uint8_t* block, const MapCapacities& capacities, int tile_width, int tile_height);

} // namespace nnh::detail
