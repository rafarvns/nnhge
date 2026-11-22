#pragma once
// One whole map conversion: a Tiled map file → `.nnhmap` bytes and the files it was made from. No disk writes (the
// CLI writes); files are read through a FileSource, so tests and fuzz targets convert from memory.

#include <cstdint>
#include <string>
#include <vector>

#include "convert_context.hpp"
#include "files.hpp"

namespace nnh::assetc {

struct Converted {
    std::vector<std::uint8_t> bytes;       ///< the `.nnhmap`, already checked by the engine's validator
    std::vector<std::string> dependencies; ///< the map, then every tileset and image it read (normalized paths)
};

/// Converts `map_path`; tileset images must be inside `assets_root`. False with `problem` set (`file: reason`).
bool convert_map(FileSource& files, const std::string& map_path, const std::string& assets_root, Converted& out,
                 Problem& problem);

} // namespace nnh::assetc
