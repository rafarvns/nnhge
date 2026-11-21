#pragma once
// The visible cells of one (map layer, tileset) → tile quads for platform::draw_tiles (spec 012 FR-2, FR-3,
// decision D26). Pure; unit tested in tests/tilemap/test_tile_expand.cpp.
//
// Per cell: one id test, one table read and one 10-byte write. The source position comes from the map's tile
// table (no division at draw time: the ARM11 has none), the orientation is the cell's top 3 bits.

#include "nnh/platform/video.hpp"
#include "tilemap/map_format.hpp"
#include "tilemap/visible_range.hpp"

#include <cstdint>

namespace nnh::detail {

struct ExpandInput {
    const std::uint16_t* cells = nullptr;        ///< the layer, row-major
    int map_width = 0;                           ///< cells per row
    const mapfmt::TileSource* sources = nullptr; ///< indexed by tile id
    int first_id = 1; ///< this pass's tileset: ids [first_id, first_id + tile_count)
    int tile_count = 0;
    LayerView view{};
    int tile_w = 16, tile_h = 16;
};

struct ExpandResult {
    int written = 0; ///< quads in `out`
    int dropped = 0; ///< tiles of this pass that did not fit in `capacity`
};

/// Writes a quad for every cell of the view holding a tile of this pass's tileset, row by row.
ExpandResult expand_tiles(const ExpandInput& in, platform::TileQuad2D* out, int capacity);

} // namespace nnh::detail
