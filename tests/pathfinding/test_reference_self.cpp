// The test references on hand-computed tiny cases (spec 025 T2.1): they agree with each other and with the numbers.

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::Diagonals;
using nnh::kUnreachable;
using nnh::PathStatus;

namespace {

path_ref::RefMap rows(const char* const* r, int h, Diagonals mode = Diagonals::OnlyIfNoObstacles) {
    return path_ref::ref_of(path_test::ascii_map(r, h, mode));
}

} // namespace

NNH_TEST(path_reference_costs_a_straight_corridor_with_weights) {
    const char* r[] = {"S9G"};
    const path_ref::RefMap m = rows(r, 1);
    const auto d = path_ref::dijkstra(m, {0, 0});
    CHECK(d[0] == 0 && d[1] == 90 && d[2] == 100);
    const auto a = path_ref::literal_astar(m, {0, 0}, {2, 0});
    CHECK(a.status == PathStatus::Found && a.cost == 100 && a.path.size() == 3 && a.expanded == 3);
    const auto f = path_ref::field(m, {{2, 0}}, kUnreachable);
    CHECK(f[0] == 100 && f[1] == 10 && f[2] == 0);
    const auto capped = path_ref::field(m, {{2, 0}}, 50);
    CHECK(capped[0] == kUnreachable && capped[1] == 10);
}

NNH_TEST(path_reference_follows_each_diagonal_rule_at_a_gap) {
    const char* r[] = {".#", "#."};
    CHECK(path_ref::literal_astar(rows(r, 2, Diagonals::Always), {0, 0}, {1, 1}).cost == 14);
    CHECK(path_ref::literal_astar(rows(r, 2, Diagonals::AtLeastOneWalkable), {0, 0}, {1, 1}).status ==
          PathStatus::NoPath);
    CHECK(path_ref::literal_astar(rows(r, 2), {0, 0}, {1, 1}).status == PathStatus::NoPath);
    const char* one[] = {"..", "#."};
    CHECK(path_ref::literal_astar(rows(one, 2, Diagonals::AtLeastOneWalkable), {0, 0}, {1, 1}).cost == 14);
    CHECK(path_ref::literal_astar(rows(one, 2), {0, 0}, {1, 1}).cost == 20);
    CHECK(path_ref::literal_astar(rows(one, 2, Diagonals::Never), {0, 0}, {1, 1}).cost == 20);
}

NNH_TEST(path_reference_dijkstra_and_literal_astar_agree_on_an_open_room) {
    const char* r[] = {"....", "....", "....", "...."};
    const path_ref::RefMap m = rows(r, 4);
    const auto d = path_ref::dijkstra(m, {0, 0});
    CHECK(d[15] == 42 && d[3] == 30 && d[7] == 34);
    const auto a = path_ref::literal_astar(m, {0, 0}, {3, 3});
    CHECK(a.cost == 42 && a.path.size() == 4);
    std::uint32_t cost = 0;
    CHECK(path_ref::valid_path(m, a.path.data(), static_cast<int>(a.path.size()), cost) && cost == 42);
}

NNH_TEST(path_reference_partial_picks_the_closest_reached_cell) {
    const char* r[] = {"S.#G"};
    const auto a = path_ref::literal_astar(rows(r, 1), {0, 0}, {3, 0}, {.partial = true});
    CHECK(a.status == PathStatus::Partial && a.path.size() == 2 && a.path.back() == (CellPos{1, 0}) && a.cost == 10);
    const auto none = path_ref::literal_astar(rows(r, 1), {0, 0}, {3, 0});
    CHECK(none.status == PathStatus::NoPath && none.path.empty());
}

NNH_TEST(path_reference_validator_refuses_bad_steps) {
    const char* r[] = {"..", "#."};
    const path_ref::RefMap m = rows(r, 2);
    const CellPos cut[] = {{0, 0}, {1, 1}};
    const CellPos jump[] = {{0, 0}, {0, 2}};
    const CellPos wall[] = {{0, 0}, {0, 1}};
    std::uint32_t cost = 0;
    CHECK(!path_ref::valid_path(m, cut, 2, cost));
    CHECK(!path_ref::valid_path(m, jump, 2, cost));
    CHECK(!path_ref::valid_path(m, wall, 2, cost));
}

NNH_TEST(path_reference_segment_checker_sees_crossed_cells_and_corners) {
    const char* open[] = {"...", "...", "..."};
    const char* side[] = {".#.", "...", "..."};
    const char* heavy[] = {"...", ".5.", "..."};
    const char* off[] = {"....", "....", "#..."};
    CHECK(path_ref::segment_ok(rows(open, 3), {0, 0}, {2, 2}, 1));
    CHECK(!path_ref::segment_ok(rows(side, 3), {0, 0}, {2, 2}, 9));            // corner (1, 1): side (1, 0) blocked
    CHECK(path_ref::segment_ok(rows(side, 3, Diagonals::Always), {0, 0}, {2, 2}, 9));
    CHECK(path_ref::segment_ok(rows(side, 3, Diagonals::AtLeastOneWalkable), {0, 0}, {2, 2}, 9));
    CHECK(!path_ref::segment_ok(rows(heavy, 3), {0, 0}, {2, 2}, 4));
    CHECK(path_ref::segment_ok(rows(heavy, 3), {0, 0}, {2, 2}, 5));
    CHECK(path_ref::segment_ok(rows(off, 3), {0, 1}, {3, 2}, 1)); // passes beside (0, 2) without crossing it
    CHECK(!path_ref::segment_ok(rows(off, 3), {1, 1}, {0, 2}, 1));
}
