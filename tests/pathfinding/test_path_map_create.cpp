// Path maps (spec 025 FR-1, FR-2, FR-6; T1.1): create() errors and limits, the 32-bit cost bound, one block of
// tagged memory, a new map all weight 1, outside blocked, moves.

#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "path_fixture.hpp"
#include "pathfinding/path_block.hpp"

using nnh::Diagonals;
using nnh::ErrorCode;
using nnh::PathMap;
using nnh::PathMapDesc;

namespace {

PathMapDesc desc(int w, int h) {
    PathMapDesc d;
    d.width = w;
    d.height = h;
    return d;
}

ErrorCode code_of(const PathMapDesc& d) {
    auto r = PathMap::create(d);
    return r ? ErrorCode::None : r.error().code;
}

} // namespace

NNH_TEST(path_map_create_accepts_sides_1_to_1024_and_refuses_others) {
    CHECK(code_of(desc(1, 1)) == ErrorCode::None);
    CHECK(code_of(desc(1024, 1)) == ErrorCode::None);
    CHECK(code_of(desc(1, 1024)) == ErrorCode::None);
    CHECK(code_of(desc(0, 5)) == ErrorCode::InvalidArgument);
    CHECK(code_of(desc(5, 0)) == ErrorCode::InvalidArgument);
    CHECK(code_of(desc(1025, 5)) == ErrorCode::InvalidArgument);
    CHECK(code_of(desc(5, 1025)) == ErrorCode::InvalidArgument);
    CHECK(code_of(desc(-3, 5)) == ErrorCode::InvalidArgument);
}

NNH_TEST(path_map_create_accepts_the_defaults_at_1024_by_1024) {
    auto r = PathMap::create(desc(1024, 1024));
    CHECK(r.ok());
    CHECK(r.value().straight_cost() == 10 && r.value().diagonal_cost() == 14);
    CHECK(r.value().diagonals() == Diagonals::OnlyIfNoObstacles && r.value().agent_size() == 1);
}

NNH_TEST(path_map_create_refuses_bad_step_costs) {
    PathMapDesc d = desc(8, 8);
    d.straight_cost = 0;
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.straight_cost = 10;
    d.diagonal_cost = 9; // below straight
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.diagonal_cost = 21; // above 2 x straight
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.diagonal_cost = 20;
    CHECK(code_of(d) == ErrorCode::None);
    d.diagonal_cost = 10;
    CHECK(code_of(d) == ErrorCode::None);
}

NNH_TEST(path_map_create_refuses_bad_agent_sizes_and_cell_sizes) {
    PathMapDesc d = desc(8, 8);
    d.agent_size = 0;
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.agent_size = 5;
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.agent_size = 4;
    CHECK(code_of(d) == ErrorCode::None);
    d.cell_w = 0;
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.cell_w = __builtin_nanf("");
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.cell_w = 16;
    d.cell_h = __builtin_inff();
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d.cell_h = -1;
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
}

NNH_TEST(path_map_create_checks_the_32_bit_cost_bound_at_its_edge) {
    // width x height x diagonal x 259 < 2^32, computed in 64 bits.
    PathMapDesc d = desc(1024, 1024);
    d.straight_cost = 10;
    d.diagonal_cost = 15; // 1024^2 x 15 x 259 = 4 073 717 760
    CHECK(code_of(d) == ErrorCode::None);
    d.diagonal_cost = 16; // 4 345 298 944
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
    d = desc(1000, 1);
    d.straight_cost = 8291;
    d.diagonal_cost = 16582; // 4 294 738 000
    CHECK(code_of(d) == ErrorCode::None);
    d.diagonal_cost = 16583; // 4 294 997 000
    CHECK(code_of(d) == ErrorCode::InvalidArgument);
}

NNH_TEST(path_map_new_is_all_weight_1_and_outside_is_blocked) {
    PathMap map = path_test::take(PathMap::create(desc(5, 3)));
    CHECK(map.valid() && map.width() == 5 && map.height() == 3);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 5; ++x) CHECK(!map.blocked(x, y) && map.weight(x, y) == 1);
    }
    CHECK(map.contains({0, 0}) && map.contains({4, 2}));
    CHECK(!map.contains({-1, 0}) && !map.contains({5, 0}) && !map.contains({0, 3}) && !map.contains({0, -1}));
    // The padded border is blocked (what searches read past the edges).
    const nnh::detail::PathMapBlock* b = nnh::detail::PathAccess::map(map);
    for (int x = -1; x <= 5; ++x) {
        CHECK(b->cells[nnh::detail::padded_index(b->stride, x, -1)] == 0);
        CHECK(b->cells[nnh::detail::padded_index(b->stride, x, 3)] == 0);
    }
    for (int y = 0; y < 3; ++y) {
        CHECK(b->cells[nnh::detail::padded_index(b->stride, -1, y)] == 0);
        CHECK(b->cells[nnh::detail::padded_index(b->stride, 5, y)] == 0);
    }
}

NNH_TEST(path_map_takes_one_block_of_its_tag_header_plus_a_byte_per_padded_cell) {
    static NNH_MEM_TAG(level_paths);
    nnh::MemTag& tag = NNH_MEM_TAG_REF(level_paths);
    PathMapDesc d = desc(20, 10);
    d.tag = &tag;
    {
        PathMap map = path_test::take(PathMap::create(d));
        CHECK(nnh::mem::tag_stats(tag).count == 1);
        CHECK(nnh::mem::tag_stats(tag).bytes == sizeof(nnh::detail::PathMapBlock) + 22 * 12);
    }
    CHECK(nnh::mem::tag_stats(tag).count == 0);
    d.agent_size = 2; // + the source bytes
    {
        PathMap map = path_test::take(PathMap::create(d));
        CHECK(nnh::mem::tag_stats(tag).bytes == sizeof(nnh::detail::PathMapBlock) + 22 * 12 + 20 * 10);
    }
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::path_memory_tag());
    {
        PathMap map = path_test::take(PathMap::create(desc(4, 4)));
        CHECK(nnh::mem::tag_stats(nnh::detail::path_memory_tag()).count == before.count + 1);
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::path_memory_tag()).count == before.count);
}

NNH_TEST(path_map_create_reports_out_of_memory) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 1024);
    CHECK(code_of(desc(512, 512)) == ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(path_map_move_leaves_the_source_invalid) {
    PathMap a = path_test::take(PathMap::create(desc(4, 4)));
    a.set_cell(1, 1, 7);
    PathMap b = static_cast<PathMap&&>(a);
    CHECK(!a.valid() && b.valid() && b.weight(1, 1) == 7);
    PathMap c;
    CHECK(!c.valid() && c.width() == 0 && c.height() == 0 && c.version() == 0);
    c = static_cast<PathMap&&>(b);
    CHECK(!b.valid() && c.valid() && c.weight(1, 1) == 7);
}

NNH_TEST(path_map_desc_mirrors_a_grid_or_a_region_of_it) {
    nnh::collision::Grid g;
    g.origin = {32, -16};
    g.cell_w = 8;
    g.cell_h = 12;
    g.width = 40;
    g.height = 30;
    const PathMapDesc all = nnh::path_map_desc(g);
    CHECK(all.width == 40 && all.height == 30 && all.grid_x == 0 && all.grid_y == 0);
    CHECK(all.origin == g.origin && all.cell_w == 8 && all.cell_h == 12);
    const PathMapDesc part = nnh::path_map_desc(g, {10, 5, 8, -1});
    CHECK(part.grid_x == 10 && part.grid_y == 5 && part.width == 8 && part.height == 25);
}
