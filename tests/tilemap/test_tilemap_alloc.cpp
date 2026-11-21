// Playing with a map allocates nothing: drawing, submitting, edits, queries and collision over 100 frames
// leave the memory stats unchanged (spec 012 FR-11, constitution V).

#include "map_fixture.hpp"
#include "nnh/memory.hpp"

using map_test::layer;
using map_test::make;
using map_test::tile;
using map_test::tileset;
using nnh::Screen;

NNH_TEST(drawing_editing_and_colliding_with_a_map_allocates_nothing_per_frame) {
    gfx_test::restart({.gfx = {.max_tiles = 4096}});
    nnh::Texture sheet = gfx_test::load("hero");
    {
        nnh::Tilemap map = make({.width = 64, .height = 32});
        tileset(map, {.texture = &sheet});
        const int back = layer(map, {.parallax = {0.5f, 0.5f}});
        const int ground = layer(map, {.collision = true});
        map.fill(back, 0, 0, 64, 32, tile(2, false, true));
        map.fill(ground, 0, 20, 64, 12, tile(1));
        nnh::Rect player{40, 100, 12, 14};
        CHECK(nnh::frame());
        const nnh::MemStats heap = nnh::mem::region_stats(nnh::MemRegion::Heap);
        const int live = nnh::mem::live_allocations();
        for (int f = 0; f < 100; ++f) {
            nnh::gfx::set_camera(Screen::Top, {{static_cast<float>(f * 3), static_cast<float>(f % 7)}});
            nnh::gfx::draw_tilemap(Screen::Top, map, {.map_layer = back});
            nnh::gfx::draw(Screen::Top, sheet, {player.x, player.y}, {.layer = 1});
            nnh::gfx::draw_tilemap(Screen::Top, map, {.map_layer = ground, .layer = 2});
            nnh::gfx::draw_tilemap(Screen::Bottom, map, {.ignore_camera = true});
            map.set_tile(ground, f % 64, 19, tile(f % 2 == 0 ? 3 : 0));
            const auto moved = nnh::collision::move(map.collision_grid(), player, {2.5f, 4});
            player.x = moved.position.x;
            player.y = moved.position.y;
            (void)map.tile_at(back, {player.x, player.y});
            CHECK(nnh::frame());
        }
        CHECK(nnh::gfx::stats().tiles_last_frame > 0);
        CHECK(nnh::mem::live_allocations() == live);
        CHECK(nnh::mem::region_stats(nnh::MemRegion::Heap).bytes == heap.bytes);
        CHECK(nnh::mem::region_stats(nnh::MemRegion::Heap).peak == heap.peak);
    }
    sheet.reset();
    nnh::shutdown();
}
