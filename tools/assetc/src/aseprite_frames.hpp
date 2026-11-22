#pragma once
// The frames and `meta` of an Aseprite sprite sheet export (spec 016 FR-17, T7.2): the JSON Hash layout (frames keyed
// by name, in file order) or the Array layout; regions, trim (`spriteSourceSize`, `sourceSize`), durations in
// milliseconds; `rotated: true` refused; `meta.image` must name the PNG beside the export and `meta.size` match it.

#include <string>

#include "convert_context.hpp"
#include "json.hpp"
#include "sprite_model.hpp"

namespace nnh::assetc {

/// Reads `frames` into `model.frames` (≤ 4096). False with `problem` (file:line: reason).
bool read_aseprite_frames(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem);

/// Checks `meta`: an object whose `image` is `png_name` and whose `size` is png_w × png_h, with every frame inside.
bool read_aseprite_meta(json::Ref root, const std::string& file, const std::string& png_name, int png_w, int png_h,
                        const SpriteModel& model, Problem& problem);

} // namespace nnh::assetc
