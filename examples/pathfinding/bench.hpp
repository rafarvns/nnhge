#pragma once
// The pathfinding bench's workloads (spec 025 AC-7), pure: seeded maps (check_maps.hpp) and a 64 x 64 tilemap built in
// code. bench_scene.cpp times one call per frame. Each returns a number that depends on the work (kept by the caller
// so nothing is optimised away); astar returns its expansions (the unit of its target).
//   astar       rooms(128, 128, 7), 32 seeded pairs: microseconds per expanded cell
//   maze_one    maze(64, 64, 8) corner to corner in one call; maze_slice: the same in slices of 256 (one per call)
//   replans     rooms(64, 64, 9), 32 pairs whose paths have 10-30 cells
//   field_one   rooms(64, 64, 9), one goal; field_slice: slices of 512
//   next        256 seeded walkable cells of that field; next_crowd: a 16 x 16 cluster (logged, not judged)
//   smooth      the first 200 cells of a seeded long path on rooms(128, 128, 7)
//   sync        a 64 x 64 PathMap from a tilemap's collision_grid(mask) with the path_cost table pattern
//   door        a 3 x 3 region sync of that map

#include <cstdint>
#include <nnh/pathfinding.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

class PathBench {
public:
    static constexpr int kPairs = 32;
    static constexpr int kAgents = 256;

    /// Builds the maps, searcher, field and tilemap (load time). False with a logged error.
    bool init();
    int astar();
    int maze_one();
    int maze_slice(); ///< 256 expansions of the running maze search (restarted when finished)
    int replans();
    int field_one();
    int field_slice(); ///< 512 cells of the running field build (restarted when finished)
    int next();
    int next_crowd();
    int smooth();
    int sync();
    int door();
    void prepare_door(); ///< untimed: toggles the 3 x 3 door's tiles
    int last_expanded() const { return last_expanded_; }

private:
    bool build_tilemap();

    std::uint8_t big_[128 * 128] = {};
    std::uint8_t maze_[64 * 64] = {};
    std::uint8_t level_[64 * 64] = {};
    int scratch_[64 * 64 / 4 + 1] = {};
    nnh::PathMap big_map_, maze_map_, level_map_, sync_map_;
    nnh::PathSearcher searcher_;
    nnh::DistanceField field_;
    nnh::DistanceField slices_;
    nnh::CellPos pairs_[kPairs][2] = {};
    nnh::CellPos replans_[kPairs][2] = {};
    nnh::CellPos agents_[kAgents] = {};
    nnh::CellPos crowd_[kAgents] = {};
    nnh::CellPos path_[128 * 128] = {};
    nnh::CellPos waypoints_[256] = {};
    int path_count_ = 0;
    nnh::CellPos goal_{};
    nnh::Texture tiles_;
    nnh::Tilemap tilemap_;
    int ground_ = -1;
    int cost_[16] = {};
    bool door_closed_ = false;
    int last_expanded_ = 0;
};
