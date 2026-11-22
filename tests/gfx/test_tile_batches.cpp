// The frame's tile batches: records, forget-by-source, expansion into the scratch within max_tiles
// (gfx/tile_batches.hpp; spec 012 FR-3, FR-11, decision D26).

#include "gfx/tile_batches.hpp"
#include "test.hpp"

using nnh::Screen;
using nnh::Vec2;
using nnh::detail::TileBatches;
using nnh::detail::TileBatchRecord;
using nnh::detail::TileExpandRequest;
using nnh::detail::TileExpandResult;
using nnh::platform::TileQuad2D;

namespace {

TileExpandRequest g_last; // what the expand function was asked last
int g_calls = 0;

/// Writes `arg` quads (as many as fit), numbered by x.
TileExpandResult write_arg_quads(const TileExpandRequest& r) {
    g_last = r;
    ++g_calls;
    TileExpandResult result;
    for (int i = 0; i < r.arg; ++i) {
        if (result.written == r.capacity) {
            ++result.dropped;
            continue;
        }
        r.out[result.written++] = {static_cast<std::int16_t>(i), 0, 0, 0, 0};
    }
    return result;
}

int kSourceA = 0; // two distinct addresses (sources are compared by address)
int kSourceB = 0;

struct Fixture {
    TileBatchRecord records[4];
    TileQuad2D scratch[8];
    TileBatches batches;

    explicit Fixture(int max_tiles = 100) {
        batches.reset(records, 4, scratch, 8, max_tiles);
        g_calls = 0;
        g_last = {};
    }
    int add(int quads, const void* source = &kSourceA, bool ignore_camera = false) {
        TileBatchRecord r;
        r.expand = write_arg_quads;
        r.source = source;
        r.arg = quads;
        r.offset = {3, 4};
        r.ignore_camera = ignore_camera;
        return batches.add(r);
    }
};

} // namespace

NNH_TEST(tile_batches_are_added_until_the_frame_is_full) {
    Fixture f;
    for (int i = 0; i < 4; ++i)
        CHECK(f.add(1) == i);
    CHECK(f.add(1) == -1);
    CHECK(f.batches.count() == 4);
    CHECK(f.batches.get(4) == nullptr && f.batches.get(-1) == nullptr);
    CHECK(f.batches.get(2) != nullptr && f.batches.get(2)->arg == 1);
}

NNH_TEST(expanding_a_batch_asks_its_function_with_the_camera_at_submit) {
    Fixture f;
    const int i = f.add(3);
    const TileBatches::Quads q = f.batches.expand(i, Screen::Bottom, {50, 60});
    CHECK(g_calls == 1);
    CHECK(g_last.source == &kSourceA && g_last.arg == 3 && g_last.screen == Screen::Bottom);
    CHECK(g_last.camera == (Vec2{50, 60}) && g_last.offset == (Vec2{3, 4}));
    CHECK(g_last.out == f.scratch && g_last.capacity == 8);
    CHECK(q.quads == f.scratch && q.count == 3 && q.quads[2].x == 2);
    CHECK(f.batches.tiles() == 3 && f.batches.dropped() == 0);
}

NNH_TEST(a_batch_that_ignores_the_camera_gets_a_zero_camera) {
    Fixture f;
    const int i = f.add(1, &kSourceA, true);
    f.batches.expand(i, Screen::Top, {50, 60});
    CHECK(g_last.camera == (Vec2{0, 0}));
}

NNH_TEST(the_budget_is_shared_by_the_frame_and_the_rest_is_dropped) {
    Fixture f(10);
    const int a = f.add(6);
    const int b = f.add(6);
    const int c = f.add(2);
    CHECK(f.batches.expand(a, Screen::Top, {}).count == 6);
    CHECK(g_last.capacity == 8); // the scratch is the smaller limit
    CHECK(f.batches.expand(b, Screen::Top, {}).count == 4);
    CHECK(g_last.capacity == 4); // 10 - 6 left
    CHECK(f.batches.expand(c, Screen::Bottom, {}).count == 0);
    CHECK(g_last.capacity == 0);
    CHECK(f.batches.tiles() == 10 && f.batches.dropped() == 4);
}

NNH_TEST(a_batch_larger_than_the_scratch_is_cut_and_counted) {
    Fixture f;
    CHECK(f.batches.expand(f.add(11), Screen::Top, {}).count == 8);
    CHECK(f.batches.tiles() == 8 && f.batches.dropped() == 3);
}

NNH_TEST(forgetting_a_source_silences_only_its_batches) {
    Fixture f;
    const int a = f.add(2, &kSourceA);
    const int b = f.add(2, &kSourceB);
    f.batches.forget(&kSourceA);
    CHECK(f.batches.get(a) == nullptr && f.batches.get(b) != nullptr);
    CHECK(f.batches.expand(a, Screen::Top, {}).count == 0);
    CHECK(g_calls == 0);
    CHECK(f.batches.expand(b, Screen::Top, {}).count == 2);
}

NNH_TEST(clear_starts_a_new_frame) {
    Fixture f(4);
    f.batches.expand(f.add(6), Screen::Top, {});
    f.batches.clear();
    CHECK(f.batches.count() == 0 && f.batches.tiles() == 0 && f.batches.dropped() == 0);
    CHECK(f.add(1) == 0);
    CHECK(f.batches.expand(0, Screen::Top, {}).count == 1);
}

NNH_TEST(without_storage_nothing_is_recorded) {
    TileBatches empty;
    empty.reset(nullptr, 0, nullptr, 0, 0);
    TileBatchRecord r;
    r.expand = write_arg_quads;
    CHECK(empty.add(r) == -1);
    CHECK(empty.expand(0, Screen::Top, {}).count == 0);
}

NNH_TEST(tile_batches_pass_the_recorded_clock_to_the_expand_function) {
    Fixture f;
    TileBatchRecord r;
    r.expand = write_arg_quads;
    r.source = &kSourceA;
    r.arg = 1;
    r.clock = 123456789012ull;
    CHECK(f.batches.add(r) == 0);
    f.batches.expand(0, Screen::Top, {});
    CHECK(g_calls == 1 && g_last.clock == 123456789012ull);
}

// Zoom (spec 018, D33).

namespace {

/// Like write_arg_quads, but stops after 5 quads when allowed to cut the batch in chunks, reporting row 5 next.
TileExpandResult write_in_chunks(const TileExpandRequest& r) {
    g_last = r;
    ++g_calls;
    TileExpandResult result;
    for (int i = r.first_row; i < r.arg && result.written < r.capacity; ++i) {
        if (r.chunk && result.written == 5) {
            result.next_row = i;
            break;
        }
        r.out[result.written++] = {static_cast<std::int16_t>(i), 0, 0, 0, 0};
    }
    return result;
}

} // namespace

NNH_TEST(expanding_returns_the_destination_size_the_expand_function_gives) {
    Fixture f;
    TileBatchRecord r;
    r.source = &kSourceA;
    r.expand = [](const TileExpandRequest& q) {
        TileExpandResult x;
        x.written = q.capacity > 0 ? 1 : 0;
        x.dst_w = 24;
        x.dst_h = 12;
        return x;
    };
    const TileBatches::Quads q = f.batches.expand(f.batches.add(r), Screen::Top, {{}, 1.5f});
    CHECK(q.count == 1 && q.dst_w == 24 && q.dst_h == 12);
}

NNH_TEST(expanding_passes_the_zoom_and_zoom_1_when_ignoring_the_camera) {
    Fixture f;
    const int a = f.add(1);
    const int b = f.add(1, &kSourceA, true);
    f.batches.expand(a, Screen::Top, {{5, 6}, 2});
    CHECK(g_last.zoom == 2 && g_last.camera == (Vec2{5, 6}));
    f.batches.expand(b, Screen::Top, {{5, 6}, 2});
    CHECK(g_last.zoom == 1 && g_last.camera == (Vec2{0, 0}));
}

NNH_TEST(a_batch_can_be_expanded_in_row_chunks_when_the_scratch_is_the_limit) {
    Fixture f;
    TileBatchRecord r;
    r.expand = write_in_chunks;
    r.source = &kSourceA;
    r.arg = 12;
    const int i = f.batches.add(r);
    TileBatches::Quads q = f.batches.expand(i, Screen::Top, {{}, 0.5f});
    CHECK(g_last.chunk && g_last.first_row == 0);
    CHECK(q.count == 5 && q.next_row == 5);
    q = f.batches.expand(i, Screen::Top, {{}, 0.5f}, q.next_row);
    CHECK(g_last.first_row == 5 && q.count == 5 && q.quads[0].x == 5 && q.next_row == 10);
    q = f.batches.expand(i, Screen::Top, {{}, 0.5f}, q.next_row);
    CHECK(q.count == 2 && q.next_row == -1);
    CHECK(f.batches.tiles() == 12 && f.batches.dropped() == 0);
}

NNH_TEST(when_the_budget_is_the_limit_the_batch_is_not_cut_in_chunks) {
    Fixture f(6); // budget 6 < scratch 8
    TileBatchRecord r;
    r.expand = write_in_chunks;
    r.source = &kSourceA;
    r.arg = 12;
    const TileBatches::Quads q = f.batches.expand(f.batches.add(r), Screen::Top, {{}, 0.5f});
    CHECK(!g_last.chunk && g_last.capacity == 6 && q.next_row == -1);
}
