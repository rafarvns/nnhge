#include "bench.hpp"

#include <utility>

#include "check_maps.hpp"
#include "demos.hpp"

namespace pd = pathfinding_demo;

namespace {

bool make_map(nnh::PathMap& out, int w, int h, const std::uint8_t* bytes) {
    auto map = nnh::PathMap::create({.width = w, .height = h});
    if (!map) return false;
    out = std::move(map.value());
    const pd::ByteGrid g{bytes, w, h};
    out.sync(g.grid(), g.weights());
    return true;
}

nnh::CellPos seeded(const std::uint8_t* bytes, int w, int h, std::uint32_t i, std::uint32_t salt) {
    const std::uint32_t r = pd::hash4(i, salt, 0, 9);
    return pd::walkable_from(bytes, w, h, {static_cast<int>(r % static_cast<std::uint32_t>(w)),
                                           static_cast<int>((r >> 16) % static_cast<std::uint32_t>(h))});
}

int tile_cost(const void* context, int x, int y) {
    const auto* b = static_cast<const std::pair<const nnh::Tilemap*, const int*>*>(context);
    return b->second[b->first->tile(0, x, y).id & 15]; // the documented table pattern (layer 0: ground)
}

} // namespace

bool PathBench::init() {
    pd::rooms(128, 128, 7, big_);
    pd::maze(64, 64, 8, maze_, scratch_);
    pd::rooms(64, 64, 9, level_);
    auto s = nnh::PathSearcher::create(128, 128);
    auto f = nnh::DistanceField::create({.width = 64, .height = 64});
    auto g = nnh::DistanceField::create({.width = 64, .height = 64});
    if (!s || !f || !g || !make_map(big_map_, 128, 128, big_) || !make_map(maze_map_, 64, 64, maze_) ||
        !make_map(level_map_, 64, 64, level_)) {
        NNH_LOG_ERROR(example, "pathfinding bench: no memory");
        return false;
    }
    searcher_ = std::move(s.value());
    field_ = std::move(f.value());
    slices_ = std::move(g.value());
    for (std::uint32_t i = 0; i < kPairs; ++i) {
        pairs_[i][0] = seeded(big_, 128, 128, i, 1);
        pairs_[i][1] = seeded(big_, 128, 128, i, 2);
    }
    // Typical enemy re-plans: seeded pairs whose paths have 10-30 cells.
    int found = 0;
    for (std::uint32_t i = 0; found < kPairs && i < 100000; ++i) {
        const nnh::CellPos a = seeded(level_, 64, 64, i, 3);
        const nnh::CellPos b = seeded(level_, 64, 64, i, 4);
        const nnh::PathResult r = searcher_.find(level_map_, a, b);
        if (r.status == nnh::PathStatus::Found && r.cells >= 10 && r.cells <= 30) {
            replans_[found][0] = a;
            replans_[found++][1] = b;
        }
    }
    goal_ = seeded(level_, 64, 64, 7, 5);
    for (std::uint32_t i = 0; i < kAgents; ++i) {
        agents_[i] = seeded(level_, 64, 64, i, 6);
        crowd_[i] = pd::walkable_from(level_, 64, 64, {20 + static_cast<int>(i % 16), 20 + static_cast<int>(i / 16)});
    }
    field_.build(level_map_, &goal_, 1);
    // A long path for smoothing: across the map and back (two searches joined at the far side), its first 200 cells.
    const nnh::CellPos a = pd::walkable_from(big_, 128, 128, {2, 2});
    const nnh::CellPos b = pd::walkable_from(big_, 128, 128, {124, 60});
    const nnh::CellPos c = pd::walkable_from(big_, 128, 128, {2, 124});
    searcher_.find(big_map_, a, b);
    path_count_ = searcher_.path(path_, 128 * 128);
    searcher_.find(big_map_, b, c);
    const int back = searcher_.path(path_ + path_count_ - 1, 128 * 128 - path_count_); // b is shared
    path_count_ += back - 1;
    if (path_count_ > 200) path_count_ = 200;
    const nnh::PathResult maze = searcher_.find(maze_map_, {1, 1}, {61, 61});
    int replan_expanded = 0;
    for (const auto& p : replans_) replan_expanded += searcher_.find(level_map_, p[0], p[1]).expanded;
    const int astar_expanded = astar();
    NNH_LOG_INFO(example,
                 "pathfinding bench: maze path %d cells, %d expanded; smooth path %d cells; replans %d pairs, %d "
                 "expanded; astar %d expanded",
                 maze.cells, maze.expanded, path_count_, found, replan_expanded, astar_expanded);
    return found == kPairs && build_tilemap();
}

bool PathBench::build_tilemap() {
    auto t = nnh::load_texture("art/tiles");
    auto m = nnh::Tilemap::create({.width = 64, .height = 64, .max_tile_layers = 2, .max_tilesets = 1,
                                   .max_tile_ids = 8, .max_object_layers = 0, .max_objects = 0,
                                   .max_properties = 16});
    if (!t || !m) return false;
    tiles_ = std::move(t.value());
    tilemap_ = std::move(m.value());
    if (!tilemap_.add_tileset({.texture = &tiles_})) return false;
    const auto ground = tilemap_.add_tile_layer({.name = "ground"});
    const auto walls = tilemap_.add_tile_layer({.name = "walls", .collision = true});
    if (!ground || !walls) return false;
    ground_ = ground.value();
    const int ids[4][2] = {{5, 1}, {1, 2}, {4, 3}, {3, 8}}; // road, floor, swamp, water: path_cost
    for (const auto& id : ids) {
        if (!tilemap_.set_int(nnh::PropertyOwner::tile(id[0]), "path_cost", id[1])) return false;
    }
    for (int id = 1; id <= 8; ++id) cost_[id] = tilemap_.properties(nnh::PropertyOwner::tile(id)).get_int("path_cost", 1);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const std::uint8_t b = level_[y * 64 + x];
            tilemap_.set_tile(ground_, x, y, {static_cast<std::uint16_t>(b == 3 ? 4 : (b == 8 ? 3 : 5))});
            if (b == 0) tilemap_.set_tile(walls.value(), x, y, {2});
        }
    }
    auto map = nnh::PathMap::create({.width = 64, .height = 64});
    if (!map) return false;
    sync_map_ = std::move(map.value());
    return true;
}

int PathBench::astar() {
    int expanded = 0;
    for (const auto& p : pairs_) expanded += searcher_.find(big_map_, p[0], p[1]).expanded;
    last_expanded_ = expanded;
    return expanded;
}

int PathBench::maze_one() {
    return static_cast<int>(searcher_.find(maze_map_, {1, 1}, {61, 61}).cost);
}

int PathBench::maze_slice() {
    if (!searcher_.running()) searcher_.begin(maze_map_, {1, 1}, {61, 61});
    searcher_.step(256);
    return searcher_.result().expanded;
}

int PathBench::replans() {
    int sum = 0;
    for (const auto& p : replans_) sum += searcher_.find(level_map_, p[0], p[1]).cells;
    return sum;
}

int PathBench::field_one() {
    field_.build(level_map_, &goal_, 1);
    return field_.settled();
}

int PathBench::field_slice() {
    if (!slices_.building()) slices_.begin(level_map_, &goal_, 1);
    slices_.step(512);
    return slices_.settled();
}

int PathBench::next() {
    int sum = 0;
    nnh::CellPos n{};
    for (const nnh::CellPos& a : agents_) sum += field_.next(level_map_, a, n) ? n.x : 0;
    return sum;
}

int PathBench::next_crowd() {
    int sum = 0;
    nnh::CellPos n{};
    for (const nnh::CellPos& a : crowd_) sum += field_.next(level_map_, a, n) ? n.y : 0;
    return sum;
}

int PathBench::smooth() { return nnh::smooth_path(big_map_, path_, path_count_, waypoints_, 256); }

int PathBench::sync() {
    const std::pair<const nnh::Tilemap*, const int*> context{&tilemap_, cost_};
    sync_map_.sync(tilemap_.collision_grid(nnh::collision::kDefaultLayer), {tile_cost, &context});
    return static_cast<int>(sync_map_.version());
}

void PathBench::prepare_door() {
    door_closed_ = !door_closed_;
    const int walls = 1;
    for (int y = 30; y < 33; ++y) {
        for (int x = 30; x < 33; ++x) tilemap_.set_tile(walls, x, y, {static_cast<std::uint16_t>(door_closed_ ? 2 : 0)});
    }
}

int PathBench::door() {
    const std::pair<const nnh::Tilemap*, const int*> context{&tilemap_, cost_};
    sync_map_.sync(tilemap_.collision_grid(nnh::collision::kDefaultLayer), {30, 30, 3, 3}, {tile_cost, &context});
    return static_cast<int>(sync_map_.version());
}
