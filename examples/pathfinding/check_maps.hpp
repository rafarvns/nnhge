#pragma once
// Seeded maps for the pathfinding self-check, bench and tests (spec 025 AC-3, AC-7): pure, FNV-1a of (seed, x, y, salt),
// no rand(). Each writes width x height bytes, row-major: 0 = wall, 1..9 = the weight of entering the cell.

#include <cstdint>

#include "nnh/pathfinding.hpp"

namespace pathfinding_demo {

/// FNV-1a 32 of four words.
std::uint32_t hash4(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d);

/// Rooms and corridors carved from solid (8 x 8 blocks, one room of 6-7 cells a side in each, every room joined to
/// its right and lower neighbours): every walkable cell connected, about 30 % walls, weights 1 (70 %), 3 and 8.
void rooms(int width, int height, std::uint32_t seed, std::uint8_t* cells);
/// A recursive-backtracker maze on the odd cells (walls between), weights 1: one path between any two cells.
/// `scratch`: width x height / 4 + 1 ints.
void maze(int width, int height, std::uint32_t seed, std::uint8_t* cells, int* scratch);
/// Open ground with weights 1..9 in 4 x 4 patches and a walled island in the middle (its inside unreachable).
void swamp(int width, int height, std::uint32_t seed, std::uint8_t* cells);

/// A grid reading `cells` (0: Solid, else Empty) and the weights callback reading the same bytes: sync(grid, weights)
/// gives the map these cells. `cells` must outlive both.
struct ByteGrid {
    const std::uint8_t* cells = nullptr;
    int width = 0;
    int height = 0;
    nnh::collision::Grid grid() const;
    nnh::PathWeights weights() const;
};

/// The first walkable cell at or after `start` in row-major order (wrapping); `start` itself if none.
nnh::CellPos walkable_from(const std::uint8_t* cells, int width, int height, nnh::CellPos start);

} // namespace pathfinding_demo
