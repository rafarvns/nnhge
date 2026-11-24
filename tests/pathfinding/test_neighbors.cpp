// Neighbours and the heuristic (spec 025 FR-6, FR-9; T2.2): the order N, E, S, W, NE, SE, SW, NW, offsets from the
// padded stride, the side cells of each diagonal, each diagonal rule, octile/Manhattan values and the scale.

#include "path_fixture.hpp"
#include "pathfinding/heuristic.hpp"
#include "pathfinding/neighbors.hpp"

using nnh::Diagonals;
namespace d = nnh::detail;

NNH_TEST(path_neighbours_are_ordered_n_e_s_w_then_ne_se_sw_nw) {
    const int dx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
    const int dy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};
    for (int i = 0; i < 8; ++i) CHECK(d::kDirDx[i] == dx[i] && d::kDirDy[i] == dy[i]);
    CHECK(d::direction_count(Diagonals::Never) == 4);
    CHECK(d::direction_count(Diagonals::Always) == 8 && d::direction_count(Diagonals::OnlyIfNoObstacles) == 8);
}

NNH_TEST(path_neighbour_offsets_come_from_the_padded_stride) {
    int off[8];
    d::neighbour_offsets(12, off);
    const int expected[8] = {-12, 1, 12, -1, -11, 13, 11, -13};
    for (int i = 0; i < 8; ++i) CHECK(off[i] == expected[i]);
    // Packed (y << 16 | x) deltas move a packed cell the same way.
    const std::uint32_t c = (5u << 16) | 7u;
    for (int i = 0; i < 8; ++i) {
        const std::uint32_t n = c + d::kPackedDelta[i];
        CHECK(static_cast<int>(n & 0xFFFFu) == 7 + d::kDirDx[i] && static_cast<int>(n >> 16) == 5 + d::kDirDy[i]);
    }
}

NNH_TEST(path_diagonal_side_cells_are_the_two_orthogonal_steps_it_passes) {
    // NE passes N and E; SE: E, S; SW: S, W; NW: W, N.
    CHECK(d::kSideA[0] == 0 && d::kSideB[0] == 1);
    CHECK(d::kSideA[1] == 1 && d::kSideB[1] == 2);
    CHECK(d::kSideA[2] == 2 && d::kSideB[2] == 3);
    CHECK(d::kSideA[3] == 3 && d::kSideB[3] == 0);
}

NNH_TEST(path_diagonal_rules_gap_passes_only_always_one_wall_passes_at_least_one) {
    // Both side cells blocked (a diagonal gap between two walls).
    CHECK(d::diagonal_allowed(Diagonals::Always, false, false));
    CHECK(!d::diagonal_allowed(Diagonals::AtLeastOneWalkable, false, false));
    CHECK(!d::diagonal_allowed(Diagonals::OnlyIfNoObstacles, false, false));
    // One wall.
    CHECK(d::diagonal_allowed(Diagonals::Always, true, false));
    CHECK(d::diagonal_allowed(Diagonals::AtLeastOneWalkable, false, true));
    CHECK(!d::diagonal_allowed(Diagonals::OnlyIfNoObstacles, true, false));
    CHECK(!d::diagonal_allowed(Diagonals::OnlyIfNoObstacles, false, true));
    // Open.
    CHECK(d::diagonal_allowed(Diagonals::OnlyIfNoObstacles, true, true));
    CHECK(!d::diagonal_allowed(Diagonals::Never, true, true));
}

NNH_TEST(path_heuristic_is_octile_with_8_neighbours_and_manhattan_with_4) {
    CHECK(d::path_heuristic(Diagonals::Never, 10, 14, 3, 5) == 80);
    CHECK(d::path_heuristic(Diagonals::OnlyIfNoObstacles, 10, 14, 3, 5) == 20 + 42);
    CHECK(d::path_heuristic(Diagonals::Always, 10, 14, 5, 3) == 20 + 42);
    CHECK(d::path_heuristic(Diagonals::AtLeastOneWalkable, 2, 3, 0, 0) == 0);
    CHECK(d::path_heuristic(Diagonals::AtLeastOneWalkable, 2, 3, 7, 7) == 21);
}

NNH_TEST(path_heuristic_scale_is_in_sixteenths_rounded_down) {
    CHECK(d::scale_heuristic(62, 16) == 62);
    CHECK(d::scale_heuristic(62, 24) == 93);
    CHECK(d::scale_heuristic(63, 24) == 94); // 94.5
    CHECK(d::scale_heuristic(62, 64) == 248);
    CHECK(d::scale_heuristic(0, 64) == 0);
}
