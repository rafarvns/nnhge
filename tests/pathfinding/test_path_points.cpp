// Cells and world points (spec 025 FR-8; T1.5): cell_at equals collision::cell_at on the same grid, the anchor shift
// of bigger agents, cell_center then cell_at gives the cell back, NaN points, cells_to_points' buffer contract.

#include "nnh/collision.hpp"
#include "path_fixture.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::Vec2;

namespace {

PathMap make(const nnh::collision::Grid& g, int k = 1, nnh::CellRect region = {0, 0, -1, -1}) {
    nnh::PathMapDesc d = nnh::path_map_desc(g, region);
    d.agent_size = k;
    return path_test::take(PathMap::create(d));
}

nnh::collision::Grid odd_grid() {
    nnh::collision::Grid g;
    g.origin = {-37.25f, 12.5f};
    g.cell_w = 12;
    g.cell_h = 9.5f;
    g.width = 40;
    g.height = 30;
    return g;
}

float coord(unsigned r, float lo, float span) { return lo + static_cast<float>(r % 100000u) * (span / 100000.0f); }

} // namespace

NNH_TEST(path_cell_at_equals_collision_cell_at_for_10000_seeded_points) {
    const nnh::collision::Grid g = odd_grid();
    const PathMap map = make(g);
    const PathMap part = make(g, 1, {5, 7, 10, 10});
    for (unsigned i = 0; i < 10000; ++i) {
        const Vec2 p{coord(path_test::hash4(i, 1, 0, 0), -100, 700), coord(path_test::hash4(i, 2, 0, 0), -50, 400)};
        const nnh::collision::CellHit hit = nnh::collision::cell_at(g, p);
        const CellPos c = map.cell_at(p);
        CHECK(c.x == hit.x && c.y == hit.y);
        const CellPos q = part.cell_at(p);
        CHECK(q.x == hit.x - 5 && q.y == hit.y - 7);
    }
    // Exact edges: the half-open rule.
    CHECK(map.cell_at({-37.25f + 12, 12.5f}).x == 1);
    CHECK(map.cell_at({-37.25f + 12, 12.5f + 9.5f}).y == 1);
}

NNH_TEST(path_cell_center_then_cell_at_gives_the_cell_back_for_every_agent_size) {
    const nnh::collision::Grid g = odd_grid();
    for (int k = 1; k <= 4; ++k) {
        const PathMap map = make(g, k, {3, 2, 20, 20});
        for (int y = 0; y < 20; ++y) {
            for (int x = 0; x < 20; ++x) CHECK(map.cell_at(map.cell_center({x, y})) == (CellPos{x, y}));
        }
    }
}

NNH_TEST(path_cell_center_is_the_middle_of_the_agents_footprint) {
    nnh::collision::Grid g;
    g.origin = {100, 50};
    g.width = 10;
    g.height = 10;
    const PathMap one = make(g);
    const PathMap two = make(g, 2);
    const PathMap region = make(g, 1, {2, 3, 4, 4});
    CHECK(one.cell_center({0, 0}) == (Vec2{108, 58}));
    CHECK(one.cell_center({3, 1}) == (Vec2{156, 74}));
    CHECK(two.cell_center({0, 0}) == (Vec2{116, 66}));
    CHECK(region.cell_center({0, 0}) == (Vec2{140, 106}));
    // The anchor of a 2 x 2 agent centred anywhere inside its top-left quarter cell rounds to that cell.
    CHECK(two.cell_at({116, 66}) == (CellPos{0, 0}));
    CHECK(two.cell_at({123.9f, 73.9f}) == (CellPos{0, 0}));
    CHECK(two.cell_at({124, 66}) == (CellPos{1, 0}));
    CHECK(two.cell_at({107, 57}) == (CellPos{-1, -1}));
}

NNH_TEST(path_cells_to_points_follows_the_buffer_contract) {
    nnh::collision::Grid g;
    g.width = 8;
    g.height = 8;
    const PathMap map = make(g);
    const CellPos cells[3] = {{0, 0}, {1, 0}, {2, 1}};
    Vec2 out[3] = {};
    CHECK(nnh::cells_to_points(map, cells, 3, out, 3) == 3);
    CHECK(out[0] == (Vec2{8, 8}) && out[1] == (Vec2{24, 8}) && out[2] == (Vec2{40, 24}));
    Vec2 small[3] = {{}, {}, {-1, -1}};
    CHECK(nnh::cells_to_points(map, cells, 3, small, 2) == 3);
    CHECK(small[1] == (Vec2{24, 8}) && small[2] == (Vec2{-1, -1}));
    CHECK(nnh::cells_to_points(map, cells, 3, nullptr, 0) == 3);
}

namespace {

const PathMap* g_map = nullptr;
CellPos g_cell{};
int g_count = 0;
void nan_point() { g_cell = g_map->cell_at({__builtin_nanf(""), 0}); }
void inf_point() { g_cell = g_map->cell_at({0, -__builtin_inff()}); }
void null_out() {
    const CellPos c[1] = {};
    g_count = nnh::cells_to_points(*g_map, c, 1, nullptr, 1);
}
void negative_count() {
    Vec2 out[1];
    g_count = nnh::cells_to_points(*g_map, nullptr, -1, out, 1);
}

} // namespace

NNH_TEST(path_point_misuse_asserts_and_answers_outside_in_release) {
    nnh::collision::Grid g;
    g.width = 4;
    g.height = 4;
    const PathMap map = make(g);
    g_map = &map;
    g_cell = {7, 7};
    CHECK(path_test::asserts_or_is_ignored(nan_point));
    if (!nnh::build::kAssertsEnabled) CHECK(g_cell == (CellPos{-1, -1}));
    CHECK(path_test::asserts_or_is_ignored(inf_point));
    g_count = 5;
    CHECK(path_test::asserts_or_is_ignored(null_out));
    if (!nnh::build::kAssertsEnabled) CHECK(g_count == 0);
    CHECK(path_test::asserts_or_is_ignored(negative_count));
    if (!nnh::build::kAssertsEnabled) CHECK(g_count == 0);
}
