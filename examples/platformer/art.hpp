#pragma once
// The example's art (art/make_art.py) and the tile ids of tiles.png. Every map adds tiles.png as its first
// tileset, so tile ids are the tile's index in the sheet + 1.

#include <cstdint>
#include <nnh/math.hpp>
#include <nnh/texture.hpp>

struct Art {
    nnh::Texture tiles;  ///< 128x64, 16x16 tiles
    nnh::Texture player; ///< 64x16, 4 frames
    nnh::Texture turret; ///< 32x16: the base (0, 0, 16, 16) and the barrel (16, 0, 12, 4) (art/make_turret.py)
};

namespace tile {
enum : std::uint16_t {
    Grass = 1,
    Dirt,
    Bricks,
    Plank,
    Coin,
    Water,
    Water2,
    Arrow,
    Hill,
    HillTop,
    CloudLeft,
    CloudMiddle,
    CloudRight,
    Bush,
    Flower,
    Stone,
    DarkBricks,
    Lattice,
    Star,
    Tuft,
};
inline constexpr int kCount = 20; ///< tiles in the sheet (the rest of it is empty)

/// Where tile `id` is in tiles.png, to draw it as a sprite.
constexpr nnh::Region region(int id) {
    return {((id - 1) % 8) * 16, ((id - 1) / 8) * 16, 16, 16};
}
} // namespace tile
