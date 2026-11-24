#pragma once
// Reference code for the pathfinding tests (spec 025 T2.1, AC-2): plain, slow, written straight from the plan's rules
// with standard containers (tests only). A copy of a map's cells, seeded random maps, a Dijkstra, the literal reference
// A* (FR-10's order: a vector scanned for the minimum, decrease-key in place), the multi-source field, a path
// validator and an exact segment checker for smoothing.

#include <cstdint>
#include <vector>

#include "nnh/pathfinding.hpp"

namespace path_ref {

using nnh::CellPos;
using nnh::Diagonals;

/// A copy of what searches see: 0 blocked, else the weight; outside the map is blocked.
struct RefMap {
    int w = 0;
    int h = 0;
    std::vector<int> cells;
    Diagonals mode = Diagonals::OnlyIfNoObstacles;
    int straight = 10;
    int diagonal = 14;

    int at(int x, int y) const { return x < 0 || y < 0 || x >= w || y >= h ? 0 : cells[y * w + x]; }
    int index(CellPos c) const { return c.y * w + c.x; }
};

RefMap ref_of(const nnh::PathMap& map);

/// Direction d = 0..7: N, E, S, W, NE, SE, SW, NW.
extern const int kDx[8];
extern const int kDy[8];
int directions(Diagonals mode); ///< 4 or 8

/// May an agent step from (x, y) in direction d? (target walkable, the diagonal rule on the two side cells)
bool step_allowed(const RefMap& m, int x, int y, int d);
/// straight or diagonal x the target's weight
std::uint32_t step_cost(const RefMap& m, int d, int nx, int ny);
/// The unscaled heuristic from (x, y) to `to`.
std::uint32_t heuristic(const RefMap& m, int x, int y, CellPos to);

/// Minimum cost from `from` to every cell (kUnreachable where none), forward steps.
std::vector<std::uint32_t> dijkstra(const RefMap& m, CellPos from);
/// Minimum cost from every cell to the nearest goal (blocked cells, blocked goals: kUnreachable), max_cost applied.
std::vector<std::uint32_t> field(const RefMap& m, const std::vector<CellPos>& goals, std::uint32_t max_cost);

struct RefResult {
    nnh::PathStatus status = nnh::PathStatus::None;
    std::uint32_t cost = 0;
    int expanded = 0;
    std::vector<CellPos> path;
};
/// FR-10 literally (plan "A search", "Partial paths"): the result find() must give, bit for bit.
RefResult literal_astar(const RefMap& m, CellPos from, CellPos to, const nnh::PathOptions& options = {});

/// A path from `from`: adjacent steps allowed by the mode, every entered cell walkable; `cost` = its summed cost.
bool valid_path(const RefMap& m, const CellPos* cells, int count, std::uint32_t& cost);

/// Exact check of a smoothing segment between the centres of a and b: every cell whose interior it crosses (but a's)
/// walkable and no heavier than max_weight; at every grid corner it passes through, the side cells obey the mode's
/// rule (Always: none; AtLeastOneWalkable: one walkable; OnlyIfNoObstacles and Never: both).
bool segment_ok(const RefMap& m, CellPos a, CellPos b, int max_weight);

/// A seeded random map (sizes up to max_side, 0-50 % blocked, weights 1-9, every mode, agent sizes 1..max_agent,
/// a few step-cost pairs).
struct RandomMap {
    nnh::PathMap map;
    RefMap ref;
    unsigned seed = 0;
};
RandomMap random_map(unsigned seed, int max_side = 64, int max_agent = 2);
/// A seeded cell of the map (any, walkable or not).
CellPos random_cell(const RefMap& m, unsigned seed, unsigned salt);

} // namespace path_ref
