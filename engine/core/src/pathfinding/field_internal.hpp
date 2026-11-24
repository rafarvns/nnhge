#pragma once
// A distance field's block (spec 025 FR-16..FR-18, D40): one or two u32 distance arrays per padded cell (front = the
// last finished build, back = the one being built; single-buffered: one array), each with a step byte per padded cell
// (T10.3: 0 = not reached by that build — its distance word is stale, the cell is kUnreachable —, 1..8 = 1 + the
// direction next() takes from the cell, kGoalStep = a goal), the Dijkstra open set (field_heap.hpp, lazily deleted:
// stale when d != back[cell], compacted when full), and the state of the running build and of the answering one. Only
// the step bytes are cleared when a build begins.

#include <cstdint>

#include "pathfinding/field_heap.hpp"
#include "pathfinding/path_block.hpp"

namespace nnh::detail {

inline constexpr std::uint8_t kGoalStep = 9;

struct FieldBlock {
    int width = 0;
    int height = 0;
    int records = 0; ///< (width + 2) x (height + 2)
    bool double_buffered = true;
    std::uint32_t* dist[2] = {};
    std::uint8_t* steps[2] = {}; ///< per padded cell, with dist[i]
    int front = 0;               ///< the answering array (single-buffered: always 0)
    FieldKey* heap = nullptr;
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

    // The answering build.
    bool ready = false;
    bool stale = false;
    int ans_stride = 0;
    int ans_w = 0;
    int ans_h = 0;
    // Its map (PathMapBlock::serial) and the version it began at: next() reads the stored steps only while the
    // map it is asked with is that map, unchanged.
    std::uint32_t ans_serial = 0;
    std::uint32_t ans_version = 0;

    int back_index() const { return double_buffered ? 1 - front : 0; }
    std::uint32_t* back() { return dist[back_index()]; }
    std::uint8_t* back_steps() { return steps[back_index()]; }
    const std::uint32_t* answer() const { return dist[front]; }
    const std::uint8_t* answer_steps() const { return steps[front]; }
    /// The answering build's distance at padded index `c`.
    std::uint32_t answer_distance(int c) const {
        return steps[front][c] != 0 ? dist[front][c] : kUnreachable;
    }
};

/// Settles at most `budget` cells of the running build; true when it finished (field_build.cpp).
bool field_run(FieldBlock& f, int budget);
/// Lets the map go (if held).
void field_release_map(FieldBlock& f);

} // namespace nnh::detail
