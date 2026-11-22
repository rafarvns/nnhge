#pragma once
// A Tiled map file (`.tmj`, JSON) → the map model (spec 012 FR-1): orthogonal and finite, tiles of 8..64 px, at
// most 1024x1024 cells, 16 tile layers, 16 object layers and 8 tilesets (8191 tile ids). Tilesets get NNHGE's
// consecutive ids in first-gid order; layers keep Tiled's order within each kind (tile, object). Refused: other
// orientations, infinite maps, image and group layers (see tiled_layer_data.hpp, tiled_objects.hpp,
// tiled_tileset.hpp for the rest). Limits on objects, properties and strings are checked by the writer.

#include <string>

#include "convert_context.hpp"
#include "map_model.hpp"

namespace nnh::assetc {

/// Reads `map_file` (a normalized path) through ctx.files. False with ctx.problem set.
bool read_tiled_map(const std::string& map_file, Context& ctx, Map& out);

} // namespace nnh::assetc
