#pragma once
// A validated `.nnhmap` file → a map block with extra capacity (spec 012 stage 4). The file's sections are
// copied entry by entry into the new layout (only the used entries: the rest of the block stays zero, so
// add_tile_layer() and friends find clean room), each layer's cells offset is set for the new layout, and the
// runtime fields are reset (texture handles -1, owned textures none) or recomputed (collision-layer mask).
// Pure (no engine includes); tested through load_tilemap in tests/tilemap/test_map_load.cpp and fuzzed by
// tests/fuzz (tilemap_target).

#include <cstdint>

#include "tilemap/map_layout.hpp"

namespace nnh::detail {

/// Room added on top of the file's capacities (LoadTilemapOptions).
struct MapExtras {
    int tile_layers = 0, objects = 0, properties = 0, string_bytes = 0;
};

/// The capacities of the loaded map: the file's plus `extras`; the tile-property table grows with the extra
/// properties, up to the tile ids. Fails (InvalidArgument when `limit` is false) on negative extras, or
/// (LimitReached) over a format limit. `file` is a validated header.
CapacityCheck loaded_capacities(const mapfmt::Header& file, const MapExtras& extras, MapCapacities& out);

/// Copies the validated file at `file` (any alignment) into `block`: zeroed, map_layout(caps).total bytes, with
/// `caps` from loaded_capacities(). Never allocates.
void copy_map(const std::uint8_t* file, const MapCapacities& caps, std::uint8_t* block);

} // namespace nnh::detail
