#pragma once
// The map model → a `.nnhmap` file (spec 012 FR-1, FR-11). Capacities equal the counts (a loaded map gets its extra
// room from LoadTilemapOptions), equal strings are stored once, and the bytes are checked with the engine's own
// validator (tilemap/map_validate.hpp) before they are returned: a file the console would refuse is never written.

#include <cstdint>
#include <string>
#include <vector>

#include "map_model.hpp"

namespace nnh::assetc {

/// Writes `map` into `out`. False with `error` set when the map is over a format limit ("more than 4096 objects")
/// or, for a converter bug, when the validator refuses the result.
bool write_map(const Map& map, std::vector<std::uint8_t>& out, std::string& error);

} // namespace nnh::assetc
