#pragma once
// The bonus level, built entirely in code (spec 012 FR-0): no file, no editor.

#include <nnh/result.hpp>
#include <nnh/tilemap.hpp>

/// Tile layer names of the bonus level, back to front.
inline constexpr const char* kBonusLayers[] = {"clouds", "hills", "ground", "front"};

/// Builds the level on `tiles` (kept alive by the caller): parallax clouds and hills, a ground layer whose
/// tiles are marked solid or one-way, a front layer drawn over the player, a "spawn" point and "coin"
/// objects.
nnh::Result<nnh::Tilemap> build_bonus_level(const nnh::Texture& tiles);
