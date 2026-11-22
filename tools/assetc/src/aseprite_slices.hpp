#pragma once
// `meta.slices` of an Aseprite export (spec 016 FR-17, T7.3): a slice named `pivot` gives every frame's pivot, from its
// first key: the key's `pivot` (relative to the key's `bounds`), or the bottom-centre of its bounds. Other slices are
// ignored.

#include <string>

#include "convert_context.hpp"
#include "json.hpp"
#include "sprite_model.hpp"

namespace nnh::assetc {

/// Sets `model.has_pivot` and the pivot when the export has a `pivot` slice. False with `problem`.
bool read_aseprite_slices(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem);

} // namespace nnh::assetc
