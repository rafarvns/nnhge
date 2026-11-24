#pragma once
// The A* heuristic (spec 025 FR-9): octile distance with the map's step costs for 8 neighbours, Manhattan x straight
// for 4. Admissible and consistent (weights >= 1), so a scale of 16 gives minimal costs. The scale is in sixteenths:
// h' = (h x scale) >> 4, in 32 bits (the creation bound keeps h x 64 below 2^32). Integer-only, no division.

#include <cstdint>

#include "nnh/pathfinding.hpp"

namespace nnh::detail {

/// h for absolute distances dx, dy (>= 0) in cells.
inline std::uint32_t path_heuristic(Diagonals mode, std::uint32_t straight, std::uint32_t diagonal, int dx, int dy) {
    const auto x = static_cast<std::uint32_t>(dx);
    const auto y = static_cast<std::uint32_t>(dy);
    if (mode == Diagonals::Never) return straight * (x + y);
    const std::uint32_t lo = x < y ? x : y;
    const std::uint32_t hi = x < y ? y : x;
    return straight * (hi - lo) + diagonal * lo;
}

inline std::uint32_t scale_heuristic(std::uint32_t h, int scale) {
    return (h * static_cast<std::uint32_t>(scale)) >> 4;
}

} // namespace nnh::detail
