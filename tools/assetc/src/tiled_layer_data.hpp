#pragma once
// A Tiled tile layer → the map model (spec 012 FR-2, FR-3, FR-7): settings (offset, parallax, opacity, visible),
// properties (`collision`, a bool, becomes the layer's collision flag) and its cells. Data is CSV (a JSON array) or
// base64 without compression; compressed data is refused with "save the layer format as CSV". Each gid is mapped to
// NNHGE's ids through the map's tilesets with its flip bits; Tiled's hexagonal rotation bit is refused, and so are
// diagonal flips when the tiles are not square.

#include <string>

#include "convert_context.hpp"
#include "json.hpp"
#include "map_model.hpp"

namespace nnh::assetc {

/// Reads one layer of type "tilelayer" of `map` (its size and tilesets with first ids set). `file` is the map's path.
bool read_tile_layer(json::Ref layer, const Map& map, const std::string& file, Context& ctx, TileLayer& out);

} // namespace nnh::assetc
