#pragma once
// The platformer's collision layers (spec 019): what each thing is (its layers) and what it detects (its mask).
// The engine names no layer; a game declares its own like this. The maps' tiles keep the default layer, which is
// kWorld here, so the player's filtered grid is the level's whole collision.

#include <nnh/collision.hpp>

namespace layers {

using nnh::collision::Layers;

inline constexpr Layers kWorld = nnh::collision::kDefaultLayer; ///< the level's tiles
inline constexpr Layers kPlayer = nnh::collision::layer(1);
inline constexpr Layers kCoin = nnh::collision::layer(2);
inline constexpr Layers kEnemy = nnh::collision::layer(3);       ///< turrets
inline constexpr Layers kEnemyBullet = nnh::collision::layer(4);

/// The player walks on the world and collects coins.
inline constexpr Layers kPlayerMask = kWorld | kCoin;
/// An enemy bullet stops at walls and hits the player; it passes turrets and coins.
inline constexpr Layers kBulletMask = kWorld | kPlayer;

} // namespace layers
