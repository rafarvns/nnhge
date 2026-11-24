#pragma once
// The example's art and the two rooms' tile maps, built in code from art/tiles.png.

#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

inline constexpr int kRoomCellsW = 25; ///< the top screen: 400 × 240 in 16-pixel tiles
inline constexpr int kRoomCellsH = 15;
inline constexpr int kTileSquare = 11; ///< the player's square in art/tiles.png

struct World {
    nnh::Texture tiles;
    nnh::Tilemap rooms[2];
    bool build();
};
