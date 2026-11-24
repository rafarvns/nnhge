// Distance fields built in slices (spec 025 FR-17, FR-20; T5.2): slices of 1, 37 and 512 equal one call; the previous
// build answers during a double-buffered rebuild; a single-buffered field isn't ready while building; stale on an edit;
// the map destroyed mid-build.

#include <new>
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

std::vector<std::uint32_t> all(const DistanceField& f, int w, int h) {
    std::vector<std::uint32_t> v;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) v.push_back(f.distance({x, y}));
    }
    return v;
}

} // namespace

NNH_TEST(path_field_slices_of_1_37_and_512_equal_one_call) {
    DistanceField one = field(48, 48);
    DistanceField sliced = field(48, 48);
    const int sizes[3] = {1, 37, 512};
    for (unsigned seed = 0; seed < 60; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 61000, 48);
        const CellPos goals[2] = {path_ref::random_cell(m.ref, seed, 1), path_ref::random_cell(m.ref, seed, 2)};
        one.build(m.map, goals, 1 + static_cast<int>(seed % 2));
        sliced.begin(m.map, goals, 1 + static_cast<int>(seed % 2));
        while (!sliced.step(sizes[seed % 3])) CHECK(sliced.building());
        CHECK(!sliced.building() && sliced.settled() == one.settled());
        CHECK(all(sliced, m.ref.w, m.ref.h) == all(one, m.ref.w, m.ref.h));
    }
}

NNH_TEST(path_field_double_buffered_answers_the_last_build_while_the_next_runs) {
    const char* rows[] = {"..........", ".........."};
    const PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(16, 16);
    const CellPos left{0, 0};
    const CellPos right{9, 0};
    f.build(map, &left, 1);
    CHECK(f.distance({5, 0}) == 50);
    f.begin(map, &right, 1);
    CHECK(f.building() && f.ready());
    f.step(3);
    CHECK(f.distance({5, 0}) == 50 && f.distance({0, 0}) == 0 && f.settled() == 3); // still the left field
    while (!f.step(3)) {
    }
    CHECK(f.distance({5, 0}) == 40 && f.distance({9, 0}) == 0);
}

NNH_TEST(path_field_single_buffered_is_not_ready_while_building) {
    const char* rows[] = {"......"};
    const PathMap map = path_test::ascii_map(rows, 1);
    DistanceField f = field(8, 8, false);
    const CellPos goal{0, 0};
    f.build(map, &goal, 1);
    CHECK(f.ready() && f.distance({5, 0}) == 50);
    f.begin(map, &goal, 1);
    CHECK(!f.ready() && f.distance({5, 0}) == kUnreachable);
    while (!f.step(2)) {
    }
    CHECK(f.ready() && f.distance({5, 0}) == 50);
    f.begin(map, &goal, 1);
    f.cancel();
    CHECK(!f.building() && !f.ready()); // its only buffer was being rewritten
}

NNH_TEST(path_field_is_stale_when_the_map_changes_mid_build) {
    const char* rows[] = {"..........", ".........."};
    PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(16, 16);
    const CellPos goal{0, 0};
    f.begin(map, &goal, 1);
    f.step(4);
    map.set_cell(8, 0, 9);
    while (!f.step(4)) {
    }
    CHECK(f.ready() && f.stale());
    f.build(map, &goal, 1);
    CHECK(!f.stale());
}

namespace {

alignas(PathMap) unsigned char g_storage[sizeof(PathMap)];
PathMap* g_doomed = nullptr;
void destroy_the_map() { g_doomed->~PathMap(); }

} // namespace

NNH_TEST(path_field_map_destroyed_mid_build_asserts_and_the_build_ends) {
    const int live = nnh::mem::live_allocations();
    {
        DistanceField f = field(16, 16);
        const char* rows[] = {"..........", ".........."};
        g_doomed = ::new (g_storage) PathMap(path_test::ascii_map(rows, 2));
        const CellPos goal{0, 0};
        f.build(*g_doomed, &goal, 1);
        f.begin(*g_doomed, &goal, 1);
        f.step(2);
        CHECK(path_test::asserts_or_is_ignored(destroy_the_map));
        CHECK(nnh::mem::live_allocations() == live + 2);
        CHECK(f.step(10));
        CHECK(!f.building() && f.ready()); // double-buffered: the earlier build still answers
        CHECK(nnh::mem::live_allocations() == live + 1);
    }
    CHECK(nnh::mem::live_allocations() == live);
}
