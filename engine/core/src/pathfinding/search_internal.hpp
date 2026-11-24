#pragma once
// A searcher's block (spec 025 FR-7, D40): one 8-byte record per padded cell {g, meta} with meta = stamp (28 bits) |
// closed (1) | the direction the cell was entered from (3), the open set (16 bytes per cell), and the state of the
// search that runs or ran last. A record whose stamp isn't the search's is "not reached": nothing is cleared per
// search; when the stamp would wrap, every meta is zeroed once.

#include <cstdint>

#include "pathfinding/neighbors.hpp"
#include "pathfinding/open_heap.hpp"
#include "pathfinding/path_block.hpp"

namespace nnh::detail {

struct SearchRecord {
    std::uint32_t g = 0;
    std::uint32_t meta = 0;
};

inline constexpr std::uint32_t kMetaDir = 7u;
inline constexpr std::uint32_t kMetaClosed = 8u;
inline constexpr int kStampShift = 4;
inline constexpr std::uint32_t kMaxStamp = (1u << 28) - 1;

struct PathSearchBlock {
    int width = 0;   ///< the largest map it searches
    int height = 0;
    int records = 0; ///< (width + 2) x (height + 2)
    SearchRecord* record = nullptr;
    OpenHeap heap;
    std::uint32_t stamp = 0;

    // The running (or last) search.
    PathMapBlock* map = nullptr; ///< while running: counted in map->users
    bool holds_map = false;
    int stride = 0;
    int map_w = 0;
    int map_h = 0;
    std::uint32_t version = 0;
    std::uint32_t straight = 10;
    std::uint32_t diagonal = 14;
    Diagonals mode = Diagonals::OnlyIfNoObstacles;
    int scale = 16;
    int limit = 0;
    bool partial = false;
    std::uint32_t from = 0; ///< packed cells
    std::uint32_t to = 0;
    int tx = 0;
    int ty = 0;
    std::uint32_t seq = 0;
    std::uint32_t best = 0;   ///< partial: the reached cell closest to `to` (packed), its unscaled h and g
    std::uint32_t best_h = 0;
    std::uint32_t best_g = 0;
    std::uint32_t end = 0;    ///< the path's last cell (packed)
    int offsets[8] = {};
    PathResult result{};
};

inline int record_index(const PathSearchBlock& s, std::uint32_t packed) {
    return padded_index(s.stride, packed_x(packed), packed_y(packed));
}

/// Ends the running search with `status` (NoPath and LimitReached become Partial with options.partial): the stale
/// flag, the cost and the path length; lets the map go.
void search_finish(PathSearchBlock& s, PathStatus status);
/// Expands at most `budget` cells of the running search; true when it finished (search_step.cpp).
bool search_run(PathSearchBlock& s, int budget);
/// Lets the map go (if held).
void search_release_map(PathSearchBlock& s);

/// Test hook (spec 025 T3.3): sets the searcher's stamp, e.g. just below the wrap.
void path_searcher_set_stamp(PathSearcher& searcher, std::uint32_t stamp);

} // namespace nnh::detail
