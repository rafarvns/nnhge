// Distance fields in one call (spec 025 FR-16, FR-23; T5.1): one and several goals, duplicates, a maximum cost,
// weights, every mode, blocked goals ignored, blocked cells unreachable, goals outside the map, and the multi-source
// reference on 300 seeded maps; memory per cell.

#include <initializer_list>
#include <vector>

#include "nnh/memory.hpp"
#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::DistanceField;
using nnh::kUnreachable;
using nnh::PathMap;

namespace {

DistanceField field(int w, int h, bool double_buffered = true) {
    return path_test::take(DistanceField::create({.width = w, .height = h, .double_buffered = double_buffered}));
}

bool equals_reference(const DistanceField& f, const path_ref::RefMap& m, const std::vector<CellPos>& goals,
                      std::uint32_t max_cost) {
    const auto want = path_ref::field(m, goals, max_cost);
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            if (f.distance({x, y}) != want[static_cast<std::size_t>(y * m.w + x)]) return false;
        }
    }
    return true;
}

} // namespace

NNH_TEST(path_field_create_refuses_bad_sizes_and_takes_one_block) {
    CHECK(DistanceField::create({.width = 0, .height = 3}).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(DistanceField::create({.width = 3, .height = 1025}).error().code == nnh::ErrorCode::InvalidArgument);
    static NNH_MEM_TAG(fields);
    {
        DistanceField d = path_test::take(DistanceField::create({.width = 10, .height = 6, .tag = &NNH_MEM_TAG_REF(fields)}));
        const std::size_t bytes = nnh::mem::tag_stats(NNH_MEM_TAG_REF(fields)).bytes;
        CHECK(bytes >= 8u * 12 * 8 + 8u * 60 && bytes <= 8u * 12 * 8 + 8u * 60 + 256); // 2 x 4 B per padded cell, heap
        DistanceField s = path_test::take(
            DistanceField::create({.width = 10, .height = 6, .double_buffered = false, .tag = &NNH_MEM_TAG_REF(fields)}));
        const std::size_t both = nnh::mem::tag_stats(NNH_MEM_TAG_REF(fields)).bytes;
        CHECK(both - bytes >= 4u * 12 * 8 + 8u * 60 && both - bytes <= 4u * 12 * 8 + 8u * 60 + 256);
        CHECK(d.valid() && d.width() == 10 && d.height() == 6 && !d.ready());
    }
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(fields)).count == 0);
}

NNH_TEST(path_field_with_one_goal_holds_each_cells_cost_to_reach_it) {
    const char* rows[] = {"S9G"};
    const PathMap map = path_test::ascii_map(rows, 1);
    DistanceField f = field(4, 4);
    const CellPos goal{2, 0};
    f.build(map, &goal, 1);
    CHECK(f.ready() && !f.building() && !f.stale() && f.settled() == 3);
    CHECK(f.distance({2, 0}) == 0 && f.distance({1, 0}) == 10 && f.distance({0, 0}) == 100);
}

NNH_TEST(path_field_with_several_goals_takes_the_nearest_and_ignores_duplicates) {
    const char* rows[] = {"..........", ".........."};
    const PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(16, 16);
    const CellPos goals[] = {{0, 0}, {9, 1}, {0, 0}, {9, 1}};
    f.build(map, goals, 4);
    CHECK(equals_reference(f, path_ref::ref_of(map), {goals, goals + 4}, kUnreachable));
    CHECK(f.distance({4, 0}) == 40 && f.distance({5, 1}) == 40 && f.distance({9, 0}) == 10);
}

NNH_TEST(path_field_leaves_cells_beyond_the_maximum_cost_unreachable) {
    const char* rows[] = {"G........."};
    const PathMap map = path_test::ascii_map(rows, 1);
    DistanceField f = field(16, 16);
    const CellPos goal{0, 0};
    f.build(map, &goal, 1, 35);
    CHECK(f.distance({3, 0}) == 30 && f.distance({4, 0}) == kUnreachable);
}

NNH_TEST(path_field_blocked_cells_and_blocked_goals_are_unreachable) {
    const char* rows[] = {"..#..", "..#.#"};
    const PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(8, 8);
    const CellPos goals[] = {{4, 1}, {0, 0}}; // (4, 1) is blocked: ignored
    f.build(map, goals, 2);
    CHECK(f.distance({4, 1}) == kUnreachable && f.distance({2, 0}) == kUnreachable);
    CHECK(f.distance({0, 0}) == 0 && f.distance({4, 0}) == kUnreachable); // the right side can't reach (0, 0)
    const CellPos only_blocked{2, 1};
    f.build(map, &only_blocked, 1);
    CHECK(f.ready() && f.distance({0, 0}) == kUnreachable && f.settled() == 0);
}

NNH_TEST(path_field_follows_every_mode_and_weights) {
    const char* rows[] = {".#..", "#...", "..5.", "...."};
    for (nnh::Diagonals mode : {nnh::Diagonals::Never, nnh::Diagonals::Always, nnh::Diagonals::AtLeastOneWalkable,
                                nnh::Diagonals::OnlyIfNoObstacles}) {
        const PathMap map = path_test::ascii_map(rows, 4, mode);
        DistanceField f = field(4, 4);
        const CellPos goal{0, 0};
        f.build(map, &goal, 1);
        CHECK(equals_reference(f, path_ref::ref_of(map), {goal}, kUnreachable));
        CHECK((f.distance({1, 1}) == kUnreachable) == (mode != nnh::Diagonals::Always));
    }
}

NNH_TEST(path_field_equals_the_multi_source_reference_on_300_seeded_maps) {
    DistanceField f = field(64, 64);
    int bad = 0;
    for (unsigned seed = 0; seed < 300; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 51000, 64);
        std::vector<CellPos> goals;
        const unsigned n = 1 + seed % 4;
        for (unsigned k = 0; k < n; ++k) goals.push_back(path_ref::random_cell(m.ref, seed, 40 + k));
        const std::uint32_t max_cost = seed % 5 == 0 ? 150u : kUnreachable;
        f.build(m.map, goals.data(), static_cast<int>(goals.size()), max_cost);
        if (!equals_reference(f, m.ref, goals, max_cost)) ++bad;
    }
    CHECK(bad == 0);
}

namespace {

DistanceField* g_f = nullptr;
const PathMap* g_map = nullptr;
void goal_outside() {
    const CellPos goals[] = {{9, 0}, {0, 0}};
    g_f->build(*g_map, goals, 2);
}
void negative_count() { g_f->build(*g_map, nullptr, -1); }
void null_goals() { g_f->build(*g_map, nullptr, 2); }
void map_too_big() {
    const char* rows[] = {"................................"};
    const PathMap big = path_test::ascii_map(rows, 1);
    const CellPos goal{0, 0};
    g_f->build(big, &goal, 1);
}
void empty_map() {
    const CellPos goal{0, 0};
    g_f->build(PathMap{}, &goal, 1);
}
void distance_outside() { CHECK(g_f->distance({0, 5}) == kUnreachable); }
void step_negative() { g_f->step(-3); }

} // namespace

NNH_TEST(path_field_misuse_asserts_and_ignores_in_release) {
    const char* rows[] = {"....", "...."};
    const PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(8, 8);
    g_f = &f;
    g_map = &map;
    CHECK(path_test::asserts_or_is_ignored(goal_outside));
    if (!nnh::build::kAssertsEnabled) CHECK(f.ready() && f.distance({3, 1}) == 34); // the outside goal ignored
    const CellPos goal{0, 0};
    f.build(map, &goal, 1); // a ready field: distance() outside its map is misuse
    for (auto fn : {negative_count, null_goals, map_too_big, empty_map, distance_outside, step_negative}) {
        CHECK(path_test::asserts_or_is_ignored(fn));
    }
    DistanceField empty;
    CHECK(!empty.valid() && !empty.ready() && !empty.building() && empty.settled() == 0);
}
