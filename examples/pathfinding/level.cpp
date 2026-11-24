#include "level.hpp"

#include <cstring>
#include <utility>

#include "demos.hpp"

namespace {

// The weights pattern (docs/api/pathfinding.md#weights-from-tiled): look the ground tile's cost up in a table made
// once from the tiles' `path_cost` property. Called by sync() only, once per walkable cell.
int ground_cost(const void* context, int x, int y) {
    const auto& level = *static_cast<const Level*>(context);
    const int cost = level.cost[level.map.tile(level.ground, x, y).id & 15];
    return cost > 0 ? cost : 1;
}

bool make_map(nnh::PathMap& out, const Level& level, int agent) {
    nnh::PathMapDesc desc = nnh::path_map_desc(level.grid());
    desc.agent_size = agent;
    auto map = nnh::PathMap::create(desc);
    if (!map) {
        NNH_LOG_ERROR(example, "path map: %s", map.error().message);
        return false;
    }
    out = std::move(map.value());
    out.sync(level.grid(), level.weights());
    return true;
}

} // namespace

bool Level::load() {
    auto loaded = nnh::load_tilemap("levels/keep");
    if (!loaded) {
        NNH_LOG_ERROR(example, "levels/keep: %s", loaded.error().message);
        return false;
    }
    map = std::move(loaded.value());
    ground = map.find_tile_layer("ground");
    walls = map.find_tile_layer("walls");
    for (int id = 1; id <= map.tile_id_count() && id < 16; ++id) {
        const nnh::Properties p = map.properties(nnh::PropertyOwner::tile(id));
        cost[id] = p.get_int("path_cost", 0);
        if (p.get_bool("door")) door_tile = id;
    }
    int found = 0;
    for (int i = 0; i < map.object_count(); ++i) {
        const nnh::MapObject o = map.object(i);
        if (std::strcmp(o.type, "hero") == 0) hero_spawn = {o.bounds.x, o.bounds.y};
        if (std::strcmp(o.type, "crowd") == 0) crowd_area = o.bounds;
        if (std::strcmp(o.type, "door") == 0 && found < kDoors) {
            doors[found++] = {static_cast<int>(o.bounds.x) / kTile, static_cast<int>(o.bounds.y) / kTile};
        }
    }
    if (ground < 0 || walls < 0 || door_tile == 0 || found != kDoors) {
        NNH_LOG_ERROR(example, "levels/keep: missing layers, door tile or door objects");
        return false;
    }
    return make_map(small, *this, 1) && make_map(big, *this, 2);
}

nnh::collision::Grid Level::grid() const {
    nnh::collision::Grid g = map.collision_grid();
    g.outside = nnh::collision::Cell::Solid;
    return g;
}

nnh::PathWeights Level::weights() const { return {ground_cost, this}; }

void Level::toggle(int index) {
    const nnh::CellPos d = doors[index];
    open[index] = !open[index];
    map.set_tile(walls, d.x, d.y, open[index] ? nnh::Tile{} : nnh::Tile{static_cast<std::uint16_t>(door_tile)});
    // A door is one cell; the agent-2 map recomputes the footprints overlapping it.
    small.sync(grid(), {d.x, d.y, 1, 1}, weights());
    big.sync(grid(), {d.x, d.y, 1, 1}, weights());
}

void Level::set_diagonals(nnh::Diagonals mode) {
    small.set_diagonals(mode);
    big.set_diagonals(mode);
}

const char* diagonals_name(nnh::Diagonals mode) {
    switch (mode) {
    case nnh::Diagonals::Never: return "never";
    case nnh::Diagonals::Always: return "always";
    case nnh::Diagonals::AtLeastOneWalkable: return "at-least-one-walkable";
    default: return "only-if-no-obstacles";
    }
}

const char* status_name(nnh::PathStatus status) {
    switch (status) {
    case nnh::PathStatus::Found: return "found";
    case nnh::PathStatus::Partial: return "partial";
    case nnh::PathStatus::NoPath: return "no-path";
    case nnh::PathStatus::LimitReached: return "limit";
    case nnh::PathStatus::Running: return "running";
    case nnh::PathStatus::Invalid: return "invalid";
    default: return "none";
    }
}
