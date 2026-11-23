#pragma once
// The queries example's room: a 25 x 15 tilemap built in code (one top screen) with walls on the world layer,
// a pool on its own water layer and one-way planks, and 12 boxes on three layers (red, green, blue), held in the
// game's own array of structs that the queries read through a box view. Art: art/make_art.py.

#include <nnh/collision.hpp>
#include <nnh/gfx.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

namespace room {

/// The example's collision layers: the engine names none, a game declares its own.
namespace bits {
inline constexpr nnh::collision::Layers kWorld = nnh::collision::kDefaultLayer; ///< walls, planks
inline constexpr nnh::collision::Layers kWater = nnh::collision::layer(1);
inline constexpr nnh::collision::Layers kRed = nnh::collision::layer(2);
inline constexpr nnh::collision::Layers kGreen = nnh::collision::layer(3);
inline constexpr nnh::collision::Layers kBlue = nnh::collision::layer(4);
inline constexpr nnh::collision::Layers kEverything = kWorld | kWater | kRed | kGreen | kBlue;
} // namespace bits

inline constexpr int kTile = 16;
inline constexpr int kWidth = 25, kHeight = 15; ///< cells

/// Tile ids of art/tiles.png (index + 1).
enum TileId : std::uint16_t { Wall = 1, Water, Plank, Floor };

/// A box of the game: its rectangle and what it is.
struct Thing {
    nnh::Rect box;
    nnh::collision::Layers layers;
};
inline constexpr int kThings = 12;

/// Where rays start (world pixels).
inline constexpr nnh::Vec2 kEmitter{40, 40};

struct Room {
    nnh::Texture tiles;
    nnh::Tilemap map;
    Thing things[kThings] = {};
    int floor = -1, walls = -1, water = -1, planks = -1; ///< tile layers

    /// Loads the tileset and builds the map and the boxes; once, after nnh::init().
    bool build();
    /// The boxes as the queries see them: the game's array, read in place.
    nnh::collision::BoxView view() const;
    /// The room's collision for `mask`: only the world and water bits matter to tiles.
    nnh::collision::Grid grid(nnh::collision::Layers mask) const;
};

/// The colour a box is drawn with: red, green or blue by its layer.
nnh::Color thing_color(nnh::collision::Layers layers);
/// "red", "green" or "blue".
const char* thing_name(nnh::collision::Layers layers);

} // namespace room
