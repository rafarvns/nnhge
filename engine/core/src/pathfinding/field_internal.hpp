#pragma once
// A distance field's block (spec 025 FR-16, FR-17, D40): one or two u32 distance arrays per padded cell (front = the
// last finished build, back = the one being built; single-buffered: one array), the Dijkstra open set of 8-byte
// entries {d, cell} keyed (d, padded index), lazily deleted (stale when d != back[cell]) and compacted when full, and
// the state of the running build and of the answering one.

#include <cstdint>

#include "pathfinding/path_block.hpp"

namespace nnh::detail {

struct FieldEntry {
    std::uint32_t d = 0;
    std::uint32_t cell = 0; ///< padded index
};

struct FieldBlock {
    int width = 0;
    int height = 0;
    int records = 0; ///< (width + 2) x (height + 2)
    bool double_buffered = true;
    std::uint32_t* dist[2] = {};
    int front = 0; ///< the answering array (single-buffered: always 0)
    FieldEntry* heap = nullptr;
    int heap_size = 0;
    int heap_capacity = 0;

    // The running (or last) build.
    PathMapBlock* map = nullptr;
    bool holds_map = false;
    bool building = false;
    int stride = 0;
    int map_w = 0;
    int map_h = 0;
    std::uint32_t version = 0;
    std::uint32_t straight = 10;
    std::uint32_t diagonal = 14;
    Diagonals mode = Diagonals::OnlyIfNoObstacles;
    std::uint32_t max_cost = 0;
    int settled = 0;
    int offsets[8] = {};

    // The answering build.
    bool ready = false;
    bool stale = false;
    int ans_stride = 0;
    int ans_w = 0;
    int ans_h = 0;

    std::uint32_t* back() { return dist[double_buffered ? 1 - front : 0]; }
    const std::uint32_t* answer() const { return dist[front]; }
};

/// Settles at most `budget` cells of the running build; true when it finished (field_build.cpp).
bool field_run(FieldBlock& f, int budget);
/// Lets the map go (if held).
void field_release_map(FieldBlock& f);

} // namespace nnh::detail
