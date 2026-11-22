#pragma once
// Tiled tilesets → the map model (spec 012 FR-1, FR-2, FR-7): an entry of a map's "tilesets", either embedded or a
// JSON tileset file (`.tsj`/`.json`, path relative to the map). Its image must be a PNG inside the assets folder
// (path relative to the file that names it) whose real size matches Tiled's numbers and the texture rules; tiles
// must be the map's size. Tile properties `solid` and `one_way` (bools) become collision flags; the others stay
// tile properties. Refused: XML tilesets, image collections, wang sets, tile offsets, animations (stage 6).

#include <string>

#include "convert_context.hpp"
#include "json.hpp"
#include "map_model.hpp"

namespace nnh::assetc {

/// Reads one element of a map's "tilesets" array. `map_file`: the map's normalized path. Adds the files it reads to
/// ctx.dependencies. False with ctx.problem set.
bool read_tileset(json::Ref entry, const std::string& map_file, int tile_width, int tile_height, Context& ctx,
                  Tileset& out);

} // namespace nnh::assetc
