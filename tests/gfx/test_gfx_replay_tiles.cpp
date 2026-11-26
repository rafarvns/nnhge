// The held frame with a tile layer drawn through a camera (spec 030 T3.7, found by the maintainer's check: the paused
// platformer lost its tile layers): a paused tick submits the same tile batches as the frame it holds, at zoom 1 and 0.5.

#include "../tilemap/map_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/system.hpp"

namespace fake = nnh::platform_fake;
using nnh::Screen;
using nnh::platform::kDevTogglePause;

namespace {

int quads(const fake::FakeTiles& t) {
    int n = 0;
    for (int b = 0; b < t.batch_count; ++b) n += t.batches[b].batch.count;
    return n;
}

void held_tiles_equal_running(float zoom) {
    gfx_test::restart({.gfx = {.max_tiles = 8000}});
    nnh::Texture sheet = gfx_test::load("hero");
    nnh::Tilemap map = map_test::make({.width = 60, .height = 40, .tile_width = 8, .tile_height = 8});
    CHECK(map_test::tileset(map, {.texture = &sheet}) == 1);
    const int l = map_test::layer(map);
    map.fill(l, 0, 0, 60, 40, map_test::tile(3));
    nnh::gfx::set_camera(Screen::Top, {{12, 6}, zoom});
    CHECK(nnh::frame());
    nnh::gfx::draw_tilemap(Screen::Top, map);
    CHECK(nnh::frame());
    nnh::gfx::draw_tilemap(Screen::Top, map);
    const int running = quads(fake::state().last_frame_tiles);
    CHECK(running > 0);
    fake::queue_dev_requests({kDevTogglePause, 0, 0, kDevTogglePause});
    CHECK(nnh::frame()); // ends the held frame, then paused ticks replay it
    CHECK(quads(fake::state().last_frame_tiles) == running);
    CHECK(fake::state().last_frame_tiles.batch_count >= 1);
    map.reset();
    sheet.reset();
    nnh::shutdown();
}

} // namespace

NNH_TEST(gfx_replay_keeps_tile_layers_through_a_camera) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    held_tiles_equal_running(1.0f);
}

NNH_TEST(gfx_replay_keeps_tile_layers_through_a_zoomed_camera) {
    if constexpr (!nnh::build::kDevToolsEnabled) return;
    held_tiles_equal_running(0.5f);
}
