#pragma once
// Neighbours of a cell (spec 025 FR-6, FR-10): direction d = 0..7 is N, E, S, W, NE, SE, SW, NW (Never uses 0..3),
// fixed forever once the goldens are recorded; padded-index offsets, packed (y << 16 | x) deltas, the two orthogonal
// cells a diagonal step passes, and the diagonal rules. The rule is symmetric: the same two cells from either end.

#include <cstdint>

#include "nnh/pathfinding.hpp"

namespace nnh::detail {

inline constexpr int kDirDx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
inline constexpr int kDirDy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};
/// Added to a packed cell (y << 16 | x), modulo 2^32: the neighbour in that direction.
inline constexpr std::uint32_t kPackedDelta[8] = {
    0xFFFF0000u, 1u, 0x00010000u, 0xFFFFFFFFu, 0xFFFF0001u, 0x00010001u, 0x0000FFFFu, 0xFFFEFFFFu,
};
/// The side cells of diagonal 4 + i: directions kSideA[i] and kSideB[i] (NE: N, E; SE: E, S; SW: S, W; NW: W, N).
inline constexpr int kSideA[4] = {0, 1, 2, 3};
inline constexpr int kSideB[4] = {1, 2, 3, 0};

inline int direction_count(Diagonals mode) { return mode == Diagonals::Never ? 4 : 8; }

/// Padded-index offsets of the eight directions for a padded stride.
inline void neighbour_offsets(int stride, int out[8]) {
    for (int d = 0; d < 8; ++d) out[d] = kDirDy[d] * stride + kDirDx[d];
}

/// May a diagonal step pass side cells whose walkability is `a` and `b`? (Never: no diagonal step at all.)
inline bool diagonal_allowed(Diagonals mode, bool a, bool b) {
    switch (mode) {
    case Diagonals::Always: return true;
    case Diagonals::AtLeastOneWalkable: return a || b;
    case Diagonals::OnlyIfNoObstacles: return a && b;
    default: return false;
    }
}

/// A packed cell: (y << 16) | x, both 0..1023.
inline std::uint32_t pack_cell(int x, int y) {
    return (static_cast<std::uint32_t>(y) << 16) | static_cast<std::uint32_t>(x);
}
inline int packed_x(std::uint32_t c) { return static_cast<int>(c & 0xFFFFu); }
inline int packed_y(std::uint32_t c) { return static_cast<int>(c >> 16); }

} // namespace nnh::detail
