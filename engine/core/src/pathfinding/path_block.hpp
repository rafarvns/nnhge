#pragma once
// The path map's block (spec 025, D40): a header and one byte per padded cell (a blocked one-cell border all round,
// so neighbour reads never check bounds), plus the source bytes for agent sizes 2-4. Searchers and fields hold the
// block, not the PathMap object: `users` counts the sliced searches and builds running over it, and a map destroyed
// while users remain is "doomed" (kept until its last user lets go).

#include <cstdint>

#include "nnh/pathfinding.hpp"

namespace nnh {
struct MemTag;
}

namespace nnh::detail {

inline constexpr int kMaxPathSide = 1024;

struct PathMapBlock {
    int width = 0;
    int height = 0;
    int stride = 0; ///< width + 2
    int grid_x = 0;
    int grid_y = 0;
    Vec2 origin{};
    float cell_w = 16;
    float cell_h = 16;
    int straight = 10;
    int diagonal = 14;
    int agent = 1;
    Diagonals diagonals = Diagonals::OnlyIfNoObstacles;
    bool one_way_walkable = false;
    bool doomed = false;
    std::uint32_t version = 0;
    int users = 0;
    std::uint8_t* cells = nullptr;  ///< (width + 2) x (height + 2): what searches read; the border is 0
    std::uint8_t* source = nullptr; ///< width x height, agent sizes 2-4 only: the cells as synced or set
};

/// Padded index of map cell (x, y).
inline int padded_index(int stride, int x, int y) { return (y + 1) * stride + x + 1; }

/// The centre of the agent's footprint anchored at `cell`, world pixels (map_geometry.cpp).
Vec2 path_cell_center(const PathMapBlock& b, CellPos cell);

/// The engine's default tag for maps, searchers and fields.
MemTag& path_memory_tag();

/// Lets a block go: one user fewer; a doomed block with no user left is freed.
void path_map_release(PathMapBlock* block);

/// The public objects' blocks (friend of PathMap, PathSearcher and DistanceField).
struct PathAccess {
    static PathMapBlock* map(const PathMap& m) { return m.block_; }
    static PathSearchBlock* searcher(const PathSearcher& s) { return s.block_; }
    static FieldBlock* field(const DistanceField& f) { return f.block_; }
};

} // namespace nnh::detail
