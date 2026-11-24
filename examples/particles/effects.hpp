#pragma once
// The gallery's effect descriptions (spec 024 AC-4): plain nnh::ParticleDesc values built in code. The sheet
// (assets/art/particles.png, art/make_art.py): 8x8 cells on the top row (spark, smoke, ember, star, rain streak), four
// flame frames on the second row, the ship at (0, 32), a brick tile at (48, 48).

#include <nnh/particles.hpp>

namespace effects {

nnh::ParticleDesc torch();     ///< local space, rate, a gradient, flame frames over life
nnh::ParticleDesc smoke();     ///< rate, damping, a growing size
nnh::ParticleDesc rain();      ///< a box along the top, world space, a high rate
nnh::ParticleDesc exhaust();   ///< the ship's trail: world space, sub-frame spreading, inherited velocity
nnh::ParticleDesc explosion(); ///< one-shot bursts outward from a ring
nnh::ParticleDesc fountain();  ///< texture-less squares
nnh::ParticleDesc fireworks(); ///< the bottom screen's third touch effect: stars outward from a ring

/// Regions of the sheet.
inline constexpr nnh::Region kShip{0, 32, 16, 16};
inline constexpr nnh::Region kChimney{48, 48, 16, 16};

} // namespace effects
