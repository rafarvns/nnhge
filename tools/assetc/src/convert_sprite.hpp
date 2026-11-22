#pragma once
// One whole sprite conversion (spec 016 FR-17, T7.4): an Aseprite export `<name>.ase.json` (and the PNG header of
// `<name>.png` beside it) → `.nnhsprite` bytes and the files read. No disk writes; files come from a FileSource.

#include <string>

#include "convert.hpp"

namespace nnh::assetc {

/// Converts `json_path` (which must end in `.ase.json` and lie inside `assets_root`). False with `problem` set
/// (`file:line: reason`, the line when known).
bool convert_sprite(FileSource& files, const std::string& json_path, const std::string& assets_root, Converted& out,
                    Problem& problem);

} // namespace nnh::assetc
