#pragma once
// The camera example's world: a map built in code (2 × 2 rooms of 40 × 24 tiles, 1280 × 768 pixels: more than four
// top screens), the square the player moves and a second square that moves by itself. Art: art/make_art.py.

#include <nnh/math.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

namespace world {

inline constexpr int kTile = 16;
inline constexpr int kRoomW = 40, kRoomH = 24; ///< cells
inline constexpr int kCellsW = 2 * kRoomW, kCellsH = 2 * kRoomH;
inline constexpr float kSquare = 12; ///< the squares' side, pixels

/// The tileset (art/tiles) and the map. build() once after nnh::init().
struct World {
    nnh::Texture tiles;
    nnh::Tilemap map;
    bool build();
};

/// The player's square: moves by `direction` (each axis −1..1) × 2 px, kept inside `bounds`.
struct Square {
    nnh::Vec2 position{};
    void move(nnh::Vec2 direction, nnh::Rect bounds);
    nnh::Rect box() const { return {position.x, position.y, kSquare, kSquare}; }
};

/// Where the automatic target is on `frame`: around the four rooms, two pixels per frame (deterministic).
nnh::Vec2 auto_target(int frame);

/// The player's start: in the top-left room, near its middle.
inline constexpr nnh::Vec2 kStart{300, 180};

} // namespace world
