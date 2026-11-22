#pragma once
// Capacities → section offsets and the block size of a sprite library (spec 016 T1.1). Pure: nnh-assetc uses it.

#include <cstdint>

#include "animation/sprite_format.hpp"

namespace nnh::detail {

struct SpriteCapacities {
    int cap[spritefmt::kSectionCount] = {}; ///< frames, clips, clip frames, markers, states, transitions
};

struct SpriteLayout {
    std::uint32_t offset[spritefmt::kSectionCount] = {}; ///< from the start of the block
    std::uint32_t total = 0;
};

/// A static reason ("frames must be 1..4096") when a capacity is outside its range, else nullptr.
const char* check_sprite_capacities(const SpriteCapacities& caps);

/// Offsets (4-byte aligned) and total size for valid capacities.
SpriteLayout sprite_layout(const SpriteCapacities& caps);

} // namespace nnh::detail
