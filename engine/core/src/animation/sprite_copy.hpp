#pragma once
// A validated `.nnhsprite` file → a library block with room for what the game adds in code (spec 016 FR-16, FR-18;
// T6.2). Pure.

#include <cstdint>

#include "animation/sprite_format.hpp"
#include "animation/sprite_layout.hpp"

namespace nnh::detail {

/// The capacities of a loaded library: the file's counts plus `extras` (frames at least 1). nullptr when fine, else
/// why, with `limit` set when a ceiling is the problem (negative extras are not).
const char* sprite_loaded_capacities(const spritefmt::Header& file, const int (&extras)[spritefmt::kSectionCount],
                                     SpriteCapacities& out, bool& limit);

/// Writes the block of `caps` at `out` (sprite_layout(caps).total bytes, zeroed by the caller) from a validated file:
/// the header with the new capacities and offsets (CRC and serial 0), then each section's records.
void copy_sprite_library(const std::uint8_t* file, const SpriteCapacities& caps, std::uint8_t* out);

} // namespace nnh::detail
