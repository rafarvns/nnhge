// The geometric rule (spec 028 T3.2, FR-16 step 4): candidates entirely beyond the focused rectangle's edge; those
// overlapping it on the cross axis first; then the smallest gap; then the smallest centre distance across; then
// pre-order. Each criterion deciding alone, ties, empty sets, all four directions.

#include "test.hpp"
#include "ui/ui_geometric.hpp"

using nnh::UiDir;
using nnh::UiRect;
using nnh::detail::geo_better;
using nnh::detail::geo_candidate;
using nnh::detail::GeoKey;

namespace {

const UiRect kFrom{100, 100, 40, 20};

/// The index of the best candidate of `rects` (pre-order = index), -1 when none qualifies.
int pick(const UiRect* rects, int n, UiDir d) {
    int best = -1;
    GeoKey best_key;
    for (int i = 0; i < n; ++i) {
        GeoKey k;
        if (!geo_candidate(kFrom, rects[i], d, i, k)) continue;
        if (best < 0 || geo_better(k, best_key)) {
            best = i;
            best_key = k;
        }
    }
    return best;
}

} // namespace

NNH_TEST(ui_geometric_only_rects_beyond_the_edge_qualify) {
    GeoKey k;
    CHECK(geo_candidate(kFrom, {100, 120, 40, 20}, UiDir::Down, 0, k));   // touching the bottom edge
    CHECK(!geo_candidate(kFrom, {100, 119, 40, 20}, UiDir::Down, 0, k));  // overlapping it
    CHECK(geo_candidate(kFrom, {100, 60, 40, 40}, UiDir::Up, 0, k));
    CHECK(!geo_candidate(kFrom, {100, 61, 40, 40}, UiDir::Up, 0, k));
    CHECK(geo_candidate(kFrom, {140, 0, 5, 5}, UiDir::Right, 0, k));
    CHECK(!geo_candidate(kFrom, {139, 0, 5, 5}, UiDir::Right, 0, k));
    CHECK(geo_candidate(kFrom, {95, 300, 5, 5}, UiDir::Left, 0, k));
    CHECK(!geo_candidate(kFrom, {96, 300, 5, 5}, UiDir::Left, 0, k));
}

NNH_TEST(ui_geometric_overlap_across_wins_over_a_smaller_gap) {
    const UiRect rects[2] = {{300, 125, 20, 20},  // gap 5, no overlap across
                             {110, 180, 20, 20}}; // gap 60, overlaps
    CHECK(pick(rects, 2, UiDir::Down) == 1);
}

NNH_TEST(ui_geometric_smaller_gap_wins_among_overlapping) {
    const UiRect rects[2] = {{100, 160, 40, 20}, {130, 140, 40, 20}};
    CHECK(pick(rects, 2, UiDir::Down) == 1);
}

NNH_TEST(ui_geometric_centre_distance_breaks_equal_gaps) {
    // both overlap and have gap 10: centres at x 150 and 116 vs 120
    const UiRect rects[2] = {{130, 130, 40, 20}, {96, 130, 40, 20}};
    CHECK(pick(rects, 2, UiDir::Down) == 1);
}

NNH_TEST(ui_geometric_pre_order_breaks_full_ties) {
    const UiRect rects[3] = {{0, 0, 1, 1}, {110, 130, 20, 20}, {110, 130, 20, 20}};
    CHECK(pick(rects, 3, UiDir::Down) == 1);
}

NNH_TEST(ui_geometric_empty_set_finds_nothing) {
    const UiRect rects[2] = {{100, 60, 40, 20}, {0, 0, 10, 10}};
    CHECK(pick(rects, 2, UiDir::Down) == -1);
    CHECK(pick(rects, 0, UiDir::Up) == -1);
}

NNH_TEST(ui_geometric_all_four_directions_on_a_cross) {
    const UiRect rects[4] = {{100, 60, 40, 20}, {100, 140, 40, 20}, {40, 100, 40, 20}, {160, 100, 40, 20}};
    CHECK(pick(rects, 4, UiDir::Up) == 0);
    CHECK(pick(rects, 4, UiDir::Down) == 1);
    CHECK(pick(rects, 4, UiDir::Left) == 2);
    CHECK(pick(rects, 4, UiDir::Right) == 3);
}

NNH_TEST(ui_geometric_gap_is_measured_between_edges) {
    // the tall one's top is nearer (gap 4) though its centre is farther than the small one's (gap 6)
    const UiRect rects[2] = {{100, 126, 40, 6}, {100, 124, 40, 80}};
    CHECK(pick(rects, 2, UiDir::Down) == 1);
}
