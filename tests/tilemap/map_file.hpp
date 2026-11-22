#pragma once
// Test-only `.nnhmap` writer (spec 012 stage 4): builds valid files with the pure layout helpers, so the
// validator and the loader can be tested before the converter (stage 5) exists. Tests damage a file through
// view() and call seal() to fix the size and CRC again, so the damage reaches the check under test.

#include <cstdint>
#include <vector>

#include "nnh/tilemap.hpp"
#include "tilemap/map_layout.hpp"

namespace map_file {

using Bytes = std::vector<std::uint8_t>;

/// A valid sample map (every section used):
/// - 8 x 4 cells of 16 x 16 px; parallax origin (8, 4); map property "music" = "theme" (string)
/// - tileset 0: image `image` (64 x 32, 8 tiles, ids 1..8, 4 columns); tile 3 solid, tile 5 one-way,
///   tile 3 property "damage" = 2 (int)
/// - tile layer 0 "back": parallax (0.5, 1), opacity 0.75; cells (0,0) = 1, (1,0) = 2 flipped x,
///   (2,1) = 5 diagonal, (3,1) = 4 flipped y; property "depth" = 1.5 (float)
/// - tile layer 1 "ground": collision layer, row 3 filled with tile 3
/// - object layer 0 "things": object 0 "spawn" (type "spawn", point at 16, 32, id 7), object 1 "coin"
///   (type "coin", rect 48, 16, 16, 16, id 9, property "value" = 5 (int)); layer property "active" = true
/// The runtime fields are as a converter writes them: no collision mask, texture slot -1.
Bytes sample(const char* image = "hero");

/// sample() with two animated tiles: tile 2 shows tiles 2 (100 ms) and 4 (50 ms), tile 6 shows tile 7 (30 ms).
Bytes animated(const char* image = "hero");

/// A minimal valid map: 1 x 1 cell of 16 x 16, no tilesets, layers, objects or properties.
Bytes minimal();

/// Two tilesets of 16 x 16 tiles: 0 = "a" (64 x 32, ids 1..8), 1 = "b" (32 x 16, ids 9..10); one tile layer of
/// 4 x 1 cells holding `cells` (ids, no flips) with tilesets mask `mask`. Valid when the mask covers the cells.
Bytes two_tilesets(const std::uint16_t (&cells)[4], std::uint8_t mask);

/// A map built in code written as a file (the block as is, runtime fields cleared, sealed). Its tilesets' names
/// are the images the loader looks for.
Bytes from_map(const nnh::Tilemap& map);

/// Writes the block size and the CRC again (call after damaging a file through view()).
void seal(Bytes& file);

inline nnh::detail::MapView view(Bytes& file) { return {file.data()}; }

} // namespace map_file
