// Query misuse and costs (spec 019 FR-17): every misuse asserts in Debug/Development and gives no hits in Release
// without reading the game's memory; no query allocates; none needs nnh::init().

#include <cstdint>

#include "alloc_counter.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/collision.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using collision::RayHit;
using nnh::Rect;
using nnh::Vec2;

namespace {

constexpr float kNan = __builtin_nanf("");
constexpr float kInf = __builtin_inff();
const Rect kBoxes[2] = {{10, 0, 10, 10}, {30, 0, 10, 10}};
const Cell kCells[4] = {Cell::Empty, Cell::Solid, Cell::OneWay, Cell::Empty};

collision::Grid grid() { return collision::grid_view(kCells, 2, 2, 16, 16); }
collision::Grid flat_grid() {
    collision::Grid g = grid();
    g.cell_w = 0;
    return g;
}
collision::BoxView boxes() { return collision::box_view(kBoxes, 2); }
// Unreadable views: a null first box with a count, a stride smaller than a Rect, a short layer stride.
collision::BoxView null_view() { return {nullptr, 3}; }
collision::BoxView short_stride() { return {kBoxes, 2, 4}; }
collision::BoxView short_layers() { return {kBoxes, 2, static_cast<int>(sizeof(Rect)), kBoxes, 2}; }

int g_result = 0;
RayHit g_hit;
RayHit g_hits[2];
collision::CellCrossing g_cells[2];

void set(bool found) { g_result = found ? 1 : 0; }

// Each case stores 1 (a hit / true) or a count in g_result: Release must give 0.
void (*const kCases[])() = {
    [] { set(collision::raycast(grid(), {kNan, 0}, {40, 8}, g_hit)); },
    [] { set(collision::raycast(grid(), {0, 8}, {kInf, 8}, g_hit)); },
    [] { set(collision::raycast(flat_grid(), {0, 8}, {40, 8}, g_hit)); },
    [] { set(collision::raycast(boxes(), {kNan, 5}, {50, 5}, g_hit)); },
    [] { set(collision::raycast(null_view(), {0, 5}, {50, 5}, g_hit)); },
    [] { set(collision::raycast(short_stride(), {0, 5}, {50, 5}, g_hit)); },
    [] { set(collision::raycast(short_layers(), {0, 5}, {50, 5}, g_hit)); },
    [] { set(collision::raycast(grid(), null_view(), {0, 5}, {50, 5}, g_hit)); },
    [] { g_result = collision::raycast_all(grid(), {0, 8}, {40, 8}, nullptr, 2); },
    [] { g_result = collision::raycast_all(grid(), {0, 8}, {40, 8}, g_hits, -1); },
    [] { g_result = collision::raycast_all(boxes(), {0, 5}, {50, 5}, nullptr, 1); },
    [] { g_result = collision::raycast_all(boxes(), {0, 5}, {50, kNan}, g_hits, 2); },
    [] { g_result = collision::cells_crossed(grid(), {0, 8}, {40, 8}, nullptr, 2); },
    [] { g_result = collision::cells_crossed(flat_grid(), {0, 8}, {40, 8}, g_cells, 2); },
    [] { g_result = collision::cells_crossed(grid(), {0, kNan}, {40, 8}, g_cells, 2); },
    [] { set(collision::line_of_sight(grid(), {kNan, 8}, {40, 8})); }, // misuse: false ("can't tell")
    [] { set(collision::line_of_sight(grid(), null_view(), {0, 8}, {40, 8})); },
    [] { set(collision::box_cast(boxes(), {0, 0, kNan, 5}, {50, 0}, g_hit)); },
    [] { set(collision::box_cast(boxes(), {0, 0, 5, 5}, {kInf, 0}, g_hit)); },
    [] { set(collision::box_cast(short_stride(), {0, 0, 5, 5}, {50, 0}, g_hit)); },
    [] { g_result = collision::box_cast_all(boxes(), {0, 0, 5, 5}, {50, 0}, nullptr, 3); },
    [] { g_result = collision::box_cast_all({kBoxes, -1}, {0, 0, 5, 5}, {50, 0}, g_hits, 2); },
};

} // namespace

NNH_TEST(every_query_misuse_asserts_in_debug_and_finds_nothing_in_release) {
    int index = 0;
    for (auto fn : kCases) {
        g_result = 7;
        const bool crashed = nnh::platform_fake::run_expecting_crash(fn);
        if (crashed != nnh::build::kAssertsEnabled) std::printf("  case %d\n", index);
        CHECK(crashed == nnh::build::kAssertsEnabled);
        if (!nnh::build::kAssertsEnabled) CHECK(g_result == 0);
        ++index;
    }
}

NNH_TEST(queries_need_no_init_and_never_allocate) {
    nnh::shutdown(); // no engine running
    const auto news = alloc_counter::news();
    const nnh::MemStats before = nnh::mem::region_stats(nnh::MemRegion::Heap);
    std::uint32_t seed = 1;
    RayHit hits[4];
    collision::CellCrossing cells[8];
    int indices[4];
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const Vec2 a{static_cast<float>(seed % 64), static_cast<float>((seed >> 8) % 40)};
        const Vec2 b{static_cast<float>((seed >> 16) % 64), static_cast<float>((seed >> 24) % 40)};
        RayHit hit;
        switch (i % 8) {
        case 0: sum += collision::raycast(grid(), a, b, hit) ? 1 : 0; break;
        case 1: sum += collision::raycast(boxes(), a, b, hit) ? 1 : 0; break;
        case 2: sum += collision::raycast(grid(), boxes(), a, b, hit) ? 1 : 0; break;
        case 3: sum += collision::raycast_all(grid(), a, b, hits, 4) + collision::raycast_all(boxes(), a, b, hits, 4); break;
        case 4: sum += collision::cells_crossed(grid(), a, b, cells, 8); break;
        case 5: sum += collision::line_of_sight(grid(), boxes(), a, b) ? 1 : 0; break;
        case 6: sum += collision::box_cast_all(boxes(), {a.x, a.y, 4, 4}, b, hits, 4); break;
        default:
            sum += collision::boxes_at(boxes(), a, indices, 4) + collision::boxes_overlapping(boxes(), {a.x, a.y, 9, 9}, indices, 4);
            sum += collision::cell_at(grid(), a).x;
            break;
        }
    }
    const nnh::MemStats after = nnh::mem::region_stats(nnh::MemRegion::Heap);
    CHECK(alloc_counter::news() == news);
    CHECK(after.count == before.count && after.bytes == before.bytes);
    CHECK(sum > 0);
}
