// Zoomed tile edges (tilemap/zoom_edges.hpp; spec 018 FR-9, decision D33): edge(i) = origin + round(i × tile × zoom)
// in 16.16 fixed point; steps are floor(tile × zoom) or one more; integer zooms give exact multiples.

#include "test.hpp"
#include "tilemap/zoom_edges.hpp"

#include <cstdint>

using nnh::detail::zoom_axis_range;
using nnh::detail::zoom_edge;
using nnh::detail::zoom_edges;
using nnh::detail::zoom_q;
using nnh::detail::zoomed_size;

NNH_TEST(zoom_q_is_the_zoom_in_16_16_fixed_point) {
    CHECK(zoom_q(1) == 65536 && zoom_q(2) == 131072 && zoom_q(0.5f) == 32768 && zoom_q(0.75f) == 49152);
    CHECK(zoom_q(1.3f) == 85197); // 1.3 × 65536 = 85196.8
}

NNH_TEST(zoomed_size_is_the_floor_of_tile_times_zoom) {
    CHECK(zoomed_size(16, zoom_q(1)) == 16 && zoomed_size(8, zoom_q(1)) == 8 && zoomed_size(64, zoom_q(1)) == 64);
    CHECK(zoomed_size(16, zoom_q(2)) == 32 && zoomed_size(16, zoom_q(0.5f)) == 8);
    CHECK(zoomed_size(16, zoom_q(1.5f)) == 24 && zoomed_size(10, zoom_q(1.5f)) == 15);
    CHECK(zoomed_size(8, zoom_q(1.3f)) == 10 && zoomed_size(9, zoom_q(0.5f)) == 4 && zoomed_size(16, zoom_q(4)) == 64);
    CHECK(zoomed_size(8, zoom_q(0.75f)) == 6);
}

NNH_TEST(zoom_edges_are_rounded_multiples_of_tile_times_zoom) {
    CHECK(zoom_edge(0, 16, zoom_q(1.5f)) == 0 && zoom_edge(1, 16, zoom_q(1.5f)) == 24);
    CHECK(zoom_edge(3, 10, zoom_q(0.75f)) == 23); // 22.5 rounds up
    CHECK(zoom_edge(5, 16, zoom_q(1.3f)) == 104);  // 5 × 20.8 = 104.0003
    CHECK(zoom_edge(7, 8, zoom_q(0.5f)) == 28);
}

NNH_TEST(every_step_is_floor_of_tile_times_zoom_or_one_more) {
    const float zooms[] = {0.5f, 0.6f, 0.75f, 0.9f, 1.1f, 1.3f, 1.5f, 1.7f, 2.25f, 2.5f, 3.3f, 3.99f};
    const int tiles[] = {8, 10, 16, 24, 32, 64};
    for (float z : zooms) {
        for (int t : tiles) {
            const int zq = zoom_q(z);
            const int d = zoomed_size(t, zq);
            for (int i = 0; i < 1100; ++i) {
                const int step = zoom_edge(i + 1, t, zq) - zoom_edge(i, t, zq);
                CHECK(step == d || step == d + 1);
            }
        }
    }
}

NNH_TEST(integer_zooms_make_exact_multiples) {
    for (int z = 1; z <= 4; ++z) {
        for (int i = 0; i < 1025; i += 7) CHECK(zoom_edge(i, 16, zoom_q(static_cast<float>(z))) == i * 16 * z);
    }
}

NNH_TEST(edge_tables_start_at_the_first_cell) {
    std::int16_t out[8] = {};
    zoom_edges(-5, 16, zoom_q(1.5f), 3, 4, out); // cells 3..6: edges 72, 96, 120, 144, 168, minus 5
    CHECK(out[0] == 67 && out[1] == 91 && out[2] == 115 && out[3] == 139 && out[4] == 163);
}

NNH_TEST(the_visible_cells_are_those_whose_span_meets_the_screen) {
    int first = 0, last = 0;
    // 24-pixel cells from −30: cell 1 spans [−6, 18), the first visible; cell 17 starts at 378 < 400, cell 18 at 402.
    zoom_axis_range(-30, 16, zoom_q(1.5f), 400, 100, first, last);
    CHECK(first == 1 && last == 18);
    // A cell ending exactly at 0 is not visible; one starting exactly at the screen's edge is not either.
    zoom_axis_range(-24, 16, zoom_q(1.5f), 24, 100, first, last);
    CHECK(first == 1 && last == 2);
    zoom_axis_range(-24, 16, zoom_q(1.5f), 25, 100, first, last);
    CHECK(first == 1 && last == 3);
}

NNH_TEST(the_visible_range_is_clamped_to_the_map) {
    int first = 0, last = 0;
    zoom_axis_range(100, 16, zoom_q(0.5f), 400, 10, first, last);
    CHECK(first == 0 && last == 10);
    zoom_axis_range(500, 16, zoom_q(0.5f), 400, 10, first, last); // entirely right of the screen
    CHECK(first == last);
    zoom_axis_range(-1000, 16, zoom_q(0.5f), 400, 10, first, last); // entirely left
    CHECK(first == last);
}

NNH_TEST(at_zoom_1_the_visible_range_is_the_unzoomed_one) {
    // Spot-check against the unzoomed rule: cells [floor(−o / t), floor((s − o − 1) / t) + 1).
    for (int origin = -100; origin <= 100; origin += 3) {
        int first = 0, last = 0;
        zoom_axis_range(origin, 16, 65536, 400, 1000, first, last);
        const int f = origin >= 0 ? 0 : (-origin) / 16;
        const int l = (400 - origin - 1) / 16 + 1;
        CHECK(first == f && last == l);
    }
}
