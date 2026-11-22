#pragma once
// `meta.frameTags` of an Aseprite export → clips and markers (spec 016 FR-17, T7.3). A tag is a clip: `name`, `from`,
// `to`, `direction` (forward, reverse, pingpong, pingpong_reverse; absent = forward) and `repeat` (a string of digits
// as Aseprite writes it, or a whole number; absent or 0 = loop): forward/reverse with repeat 1 → Once, n → Repeat n;
// ping-pong → n one-way passes (Aseprite's count). A tag named `@name` or `@name=value` (an integer from -32768 to
// 32767) is a marker on every clip frame showing its frames, in every clip. No clip tag → one clip `default`, every
// frame forward, looping. `color` and `data` (tag user data) are ignored.

#include <string>

#include "convert_context.hpp"
#include "json.hpp"
#include "sprite_model.hpp"

namespace nnh::assetc {

/// Reads the tags into `model.clips` and `model.markers` (frames read first). False with `problem`.
bool read_aseprite_tags(json::Ref root, const std::string& file, SpriteModel& model, Problem& problem);

} // namespace nnh::assetc
