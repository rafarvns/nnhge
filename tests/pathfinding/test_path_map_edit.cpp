// Path map edits and queries (spec 025 FR-2, FR-4, FR-23; T1.3): set_cell, fill (clipped), set_diagonals, blocked,
// weight, the version counter (+1 per accepted call), misuse.

#include <initializer_list>

#include "nnh/collision.hpp"
#include "path_fixture.hpp"

using nnh::Diagonals;
using nnh::kPathBlocked;
using nnh::PathMap;

namespace {

PathMap make(int w, int h) {
    nnh::PathMapDesc d;
    d.width = w;
    d.height = h;
    return path_test::take(PathMap::create(d));
}

} // namespace

NNH_TEST(path_set_cell_sets_a_weight_or_blocks_and_reads_back) {
    PathMap map = make(4, 4);
    map.set_cell(2, 1, 9);
    map.set_cell(0, 3, kPathBlocked);
    map.set_cell(3, 3, 255);
    CHECK(map.weight(2, 1) == 9 && !map.blocked(2, 1));
    CHECK(map.blocked(0, 3) && map.weight(0, 3) == kPathBlocked);
    CHECK(map.weight(3, 3) == 255);
    map.set_cell(0, 3, 1);
    CHECK(!map.blocked(0, 3) && map.weight(0, 3) == 1);
}

NNH_TEST(path_fill_sets_a_rectangle_clipped_to_the_map) {
    PathMap map = make(5, 4);
    map.fill({3, 2, 10, 10}, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 5; ++x) CHECK(map.weight(x, y) == (x >= 3 && y >= 2 ? 4 : 1));
    }
    map.fill({-2, -2, 3, 3}, kPathBlocked); // (0..0, 0..0)
    CHECK(map.blocked(0, 0) && !map.blocked(1, 0) && !map.blocked(0, 1));
    map.fill({1, 1, 0, 3}, 7); // empty: nothing
    CHECK(map.weight(1, 1) == 1);
}

NNH_TEST(path_version_grows_by_one_per_accepted_change) {
    PathMap map = make(4, 4);
    const nnh::collision::Cell cells[16] = {};
    std::uint32_t v = map.version();
    map.set_cell(1, 1, 3);
    CHECK(map.version() == ++v);
    map.fill({0, 0, 2, 2}, 2);
    CHECK(map.version() == ++v);
    map.fill({0, 0, 0, 0}, 2); // an empty region is still a call
    CHECK(map.version() == ++v);
    map.set_diagonals(Diagonals::Always);
    CHECK(map.version() == ++v && map.diagonals() == Diagonals::Always);
    map.sync(nnh::collision::grid_view(cells, 4, 4, 16, 16));
    CHECK(map.version() == ++v);
    map.sync(nnh::collision::grid_view(cells, 4, 4, 16, 16), {1, 1, 1, 1});
    CHECK(map.version() == ++v);
}

NNH_TEST(path_map_accessors_report_the_desc) {
    nnh::PathMapDesc d;
    d.width = 7;
    d.height = 3;
    d.straight_cost = 3;
    d.diagonal_cost = 4;
    d.diagonals = Diagonals::Never;
    d.agent_size = 2;
    PathMap map = path_test::take(PathMap::create(d));
    CHECK(map.width() == 7 && map.height() == 3 && map.straight_cost() == 3 && map.diagonal_cost() == 4);
    CHECK(map.diagonals() == Diagonals::Never && map.agent_size() == 2);
}

namespace {

PathMap* g_map = nullptr;
void set_outside() { g_map->set_cell(4, 0, 2); }
void set_negative() { g_map->set_cell(0, -1, 2); }
void set_zero_weight() { g_map->set_cell(0, 0, 0); }
void set_heavy_weight() { g_map->set_cell(0, 0, 256); }
void fill_zero_weight() { g_map->fill({0, 0, 2, 2}, 0); }
void fill_negative_region() { g_map->fill({0, 0, -1, 2}, 3); }
void fill_outside() { g_map->fill({4, 4, 2, 2}, 3); }
void read_blocked_outside() { CHECK(g_map->blocked(-1, 0)); }
void read_weight_outside() { CHECK(g_map->weight(0, 4) == kPathBlocked); }
void bad_diagonals() { g_map->set_diagonals(static_cast<Diagonals>(9)); }
void edit_an_empty_map() {
    PathMap empty;
    empty.set_cell(0, 0, 1);
}

} // namespace

NNH_TEST(path_map_edit_misuse_asserts_and_changes_nothing_in_release) {
    PathMap map = make(4, 4);
    g_map = &map;
    const std::uint32_t v = map.version();
    for (auto fn : {set_outside, set_negative, set_zero_weight, set_heavy_weight, fill_zero_weight,
                    fill_negative_region, fill_outside, read_blocked_outside, read_weight_outside, bad_diagonals,
                    edit_an_empty_map}) {
        CHECK(path_test::asserts_or_is_ignored(fn));
    }
    CHECK(map.version() == v);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) CHECK(map.weight(x, y) == 1);
    }
    CHECK(map.diagonals() == Diagonals::OnlyIfNoObstacles);
    PathMap empty;
    CHECK(empty.blocked(0, 0) && empty.weight(0, 0) == kPathBlocked && !empty.contains({0, 0}));
}
