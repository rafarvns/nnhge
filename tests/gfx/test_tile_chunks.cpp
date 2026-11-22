// A zoomed-out tile layer larger than the 1600-quad scratch is sent in row chunks, in order, with every tile
// (spec 018 FR-9, decision D33); the frame's max_tiles budget still drops what doesn't fit.

#include "../tilemap/map_fixture.hpp"
#include "nnh/build.hpp"

#include <cstring>

namespace fake = nnh::platform_fake;
using nnh::Screen;
namespace gfx = nnh::gfx;

namespace {

/// 8 × 8 tiles at zoom 0.5 on the top screen: 101 × 61 = 6161 visible quads (camera 2 px off the grid).
struct Wide {
    nnh::Texture sheet;
    nnh::Tilemap map;
    explicit Wide(int max_tiles) {
        gfx_test::restart({.gfx = {.max_tiles = max_tiles}});
        sheet = gfx_test::load("hero");
        map = map_test::make({.width = 200, .height = 100, .tile_width = 8, .tile_height = 8});
        CHECK(map_test::tileset(map, {.texture = &sheet}) == 1);
        const int l = map_test::layer(map);
        map.fill(l, 0, 0, 200, 100, map_test::tile(3));
    }
    ~Wide() {
        map.reset();
        sheet.reset();
        nnh::shutdown();
    }
};

Wide* g_wide = nullptr;
void draw_and_submit() {
    gfx::draw_tilemap(Screen::Top, g_wide->map);
    (void)nnh::frame();
}

} // namespace

NNH_TEST(a_batch_larger_than_the_scratch_is_drawn_in_row_chunks_in_order) {
    Wide w(8000);
    gfx::set_camera(Screen::Top, {{4, 4}, 0.5f});
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, w.map);
    CHECK(nnh::frame());
    const fake::FakeTiles& t = fake::state().last_frame_tiles;
    CHECK(t.batch_count == 5); // 15 rows of 101 (1515 quads) fit in 1600
    int total = 0;
    for (int b = 0; b < t.batch_count; ++b) {
        const int count = t.batches[b].batch.count;
        CHECK(count % 101 == 0 && t.batches[b].batch.dst_w == 4 && t.batches[b].batch.dst_h == 4);
        if (b < 4) CHECK(count == 1515);
        total += count;
    }
    CHECK(total == 6161 && gfx::stats().tiles_last_frame == 6161 && gfx::stats().dropped_tiles == 0);
    CHECK(std::strcmp(fake::state().last_frame_calls, "ttttt") == 0);
    // The first chunk's rows go top to bottom (later chunks aren't recorded: the fake keeps 512 quads).
    CHECK(t.quads[0].y == -2 && t.quads[101].y == 2 && t.quads[202].y == 6);
}

NNH_TEST(the_budget_still_drops_what_does_not_fit_after_chunks) {
    Wide w(2000);
    g_wide = &w;
    gfx::set_camera(Screen::Top, {{4, 4}, 0.5f});
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(draw_and_submit);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    CHECK(gfx::stats().tiles_last_frame == 2000 && gfx::stats().dropped_tiles == 4161);
}

NNH_TEST(at_zoom_1_and_above_a_batch_is_one_call) {
    Wide w(8000);
    gfx::set_camera(Screen::Top, {{4, 4}, 1});
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, w.map);
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_tiles.batch_count == 1 && gfx::stats().tiles_last_frame == 51 * 31);
}
