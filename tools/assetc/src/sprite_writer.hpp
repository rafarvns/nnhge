#pragma once
// A sprite model → `.nnhsprite` bytes (spec 016 FR-16, FR-17, T7.4): capacities equal to the counts, markers placed on
// every clip frame their tag covers, the pivot on every frame, the CRC; then checked with the engine's own validator.

#include <cstdint>
#include <string>
#include <vector>

#include "convert_context.hpp"
#include "sprite_model.hpp"

namespace nnh::assetc {

/// False with `problem` (`file`, the line of the tag or frame at fault) when the model breaks a limit of the format.
bool write_sprite(const SpriteModel& model, const std::string& file, std::vector<std::uint8_t>& out, Problem& problem);

} // namespace nnh::assetc
