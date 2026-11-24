#pragma once
// One whole model conversion (spec 026 FR-6, D42): a glTF 2.0 file (.gltf + its .bin files, or .glb) → `.nnhmesh` bytes
// and the files it was made from (the model, its buffers, its PNGs). No disk writes; files through a FileSource.

#include <string>
#include <vector>

#include "convert.hpp"

namespace nnh::assetc {

/// Converts `model_path` (inside `assets_root`). False with `problem` set (`file: reason (at $.json.path)`). `notes`
/// (optional) receives one line per ignored feature (skins, animations, PBR maps, …).
bool convert_mesh(FileSource& files, const std::string& model_path, const std::string& assets_root, Converted& out,
                  Problem& problem, std::vector<std::string>* notes = nullptr);

} // namespace nnh::assetc
