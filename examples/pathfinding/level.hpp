#pragma once
// The keep (spec 025 AC-4): a level made in Tiled (art/make_level.py), its ground tiles' `path_cost` property turned
// into path weights (the documented pattern: a per-tile-id table, then a two-line callback), two path maps from its
// collision grid (agent sizes 1 and 2), and three doors toggled with set_tile + a one-cell region sync on both maps.

#include <nnh/collision.hpp>
#include <nnh/pathfinding.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

struct Level {
    static constexpr int kDoors = 3;
    static constexpr int kTile = 16;

    nnh::Tilemap map;
    int ground = -1;
    int walls = -1;
    int door_tile = 0;      ///< the tile with the `door` property (a closed door)
    int cost[16] = {};      ///< path_cost per ground tile id (the pattern; 0: no property)
    nnh::PathMap small;     ///< agent size 1
    nnh::PathMap big;       ///< agent size 2
    nnh::CellPos doors[kDoors] = {};
    bool open[kDoors] = {true, true, true};
    nnh::Vec2 hero_spawn{};
    nnh::Rect crowd_area{};

    /// Loads levels/keep and builds both maps (load time: allocates). False with a logged error.
    bool load();
    /// The collision grid both maps are synced from (walls and closed doors solid, the level walled in).
    nnh::collision::Grid grid() const;
    /// The weight callback over the ground layer.
    nnh::PathWeights weights() const;
    /// Opens or closes door `index`: the tile, then a one-cell region sync of both maps.
    void toggle(int index);
    void set_diagonals(nnh::Diagonals mode);
    int width() const { return map.width(); }
    int height() const { return map.height(); }
};

const char* diagonals_name(nnh::Diagonals mode);
const char* status_name(nnh::PathStatus status);
