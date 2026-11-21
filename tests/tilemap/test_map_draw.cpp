// gfx::draw_tilemap on the fake platform: batches per (map layer, tileset), interleaved with sprites by draw
// layer and call order, camera/parallax at the end of the frame, edits seen, destroyed maps forgotten
// (spec 012 FR-3, FR-4, FR-11, decision D26).

#include "map_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/video.hpp"

#include <cstring>

namespace fake = nnh::platform_fake;
using map_test::layer;
using map_test::make;
using map_test::tile;
using map_test::tileset;
using nnh::Screen;
namespace gfx = nnh::gfx;

namespace {

/// The engine with max_tiles, a 64x32 sheet (ids 1..8, four per row) and a 30x20 map with two layers.
struct Scene {
    nnh::Texture sheet;
    nnh::Texture other;
    nnh::Tilemap map;
    int back = -1, front = -1;

    explicit Scene(int max_tiles = 2048) {
        gfx_test::restart({.gfx = {.max_tiles = max_tiles}});
        sheet = gfx_test::load("hero");
        other = gfx_test::load("tile");
        map = make({.width = 30, .height = 20});
        CHECK(tileset(map, {.texture = &sheet}) == 1);
        back = layer(map, {.name = "back"});
        front = layer(map, {.name = "front"});
    }
    ~Scene() {
        map.reset();
        sheet.reset();
        other.reset();
        nnh::shutdown();
    }
};

const char* calls() {
    return fake::state().last_frame_calls;
}
const fake::FakeTiles& tiles() {
    return fake::state().last_frame_tiles;
}
const nnh::platform::TileQuad2D& quad(int i) {
    return tiles().quads[i];
}
const nnh::platform::TileBatch2D& batch(int i) {
    return tiles().batches[i].batch;
}

Scene* g_scene = nullptr;
void draw_bad_layer() {
    gfx::draw_tilemap(Screen::Top, g_scene->map, {.map_layer = 9});
}
void draw_whole_map() {
    gfx::draw_tilemap(Screen::Top, g_scene->map);
}

} // namespace

NNH_TEST(each_visible_layer_is_one_batch_with_its_tiles_where_the_camera_sees_them) {
    Scene s;
    s.map.set_tile(s.back, 2, 1, tile(6)); // second row of the sheet, second column
    s.map.set_tile(s.front, 0, 0, tile(1, true));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "tt") == 0);
    CHECK(tiles().batch_count == 2 && tiles().quad_count == 2);
    CHECK(batch(0).count == 1 && batch(0).tile_w == 16 && batch(0).tile_h == 16 && batch(0).texture != 0);
    CHECK(quad(0).x == 32 && quad(0).y == 16 && quad(0).src_x == 16 && quad(0).src_y == 16);
    CHECK(quad(1).x == 0 && quad(1).orient == nnh::platform::kTileOrientFlipX);
    CHECK(batch(0).color.r == 255 && batch(0).color.a == 255);
}

NNH_TEST(map_layer_draws_only_that_layer_and_empty_layers_make_no_batch) {
    Scene s;
    s.map.set_tile(s.front, 1, 1, tile(2));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.map_layer = s.front});
    gfx::draw_tilemap(Screen::Bottom, s.map, {.map_layer = s.back}); // empty: nothing
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "t") == 0 && tiles().quad_count == 1);
}

NNH_TEST(a_layer_using_two_tilesets_is_two_batches_in_tileset_order) {
    Scene s;
    const int second = tileset(s.map, {.texture = &s.other}); // id 9
    s.map.set_tile(s.back, 0, 0, tile(second));
    s.map.set_tile(s.back, 1, 0, tile(3));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 2);
    CHECK(batch(0).count == 1 && quad(0).x == 16 && quad(0).src_x == 32); // the sheet's id 3
    CHECK(batch(1).count == 1 && quad(1).x == 0 && quad(1).src_x == 0);   // the 16x16 texture
    CHECK(batch(0).texture != batch(1).texture);
}

NNH_TEST(sprites_can_sit_between_map_layers) {
    Scene s;
    s.map.fill(s.back, 0, 0, 30, 20, tile(1));
    s.map.set_tile(s.front, 3, 3, tile(2));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.map_layer = s.front, .layer = 5});
    gfx::draw(Screen::Top, s.sheet, {10, 10}, {.layer = 3}); // the player
    gfx::draw_tilemap(Screen::Top, s.map, {.map_layer = s.back, .layer = 1});
    gfx::fill_rect(Screen::Top, {0, 0, 4, 4}, nnh::colors::red, {.layer = 5}); // same layer, called later
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "tTtT") == 0);
    CHECK(batch(0).layer == 1 && batch(1).layer == 5);
    CHECK(batch(0).count == 25 * 15); // camera at 0: whole tiles cover the top screen
}

NNH_TEST(the_camera_at_the_end_of_the_frame_places_the_tiles) {
    Scene s;
    s.map.set_tile(s.back, 2, 0, tile(1));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.offset = {1, 2}});
    gfx::set_camera(Screen::Top, {{24.4f, -10}});
    CHECK(nnh::frame());
    CHECK(quad(0).x == 32 + 1 - 24 && quad(0).y == 2 + 10);
}

NNH_TEST(ignore_camera_draws_the_map_in_screen_pixels) {
    Scene s;
    s.map.set_tile(s.back, 2, 0, tile(1));
    gfx::set_camera(Screen::Top, {{500, 500}});
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.ignore_camera = true});
    CHECK(nnh::frame());
    CHECK(tiles().quad_count == 1 && quad(0).x == 32 && quad(0).y == 0);
}

NNH_TEST(layer_parallax_offset_and_the_map_parallax_origin_are_applied) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    s.map.set_layer_parallax(s.back, {0, 0});
    s.map.set_layer_offset(s.back, {3, 4});
    s.map.set_parallax_origin({200, 120}); // the view centre at camera 0: a fixed layer lines up with the map
    CHECK(s.map.parallax_origin() == (nnh::Vec2{200, 120}));
    gfx::set_camera(Screen::Top, {{1000, 37}});
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    CHECK(nnh::frame());
    CHECK(tiles().quad_count == 1 && quad(0).x == 3 && quad(0).y == 4); // parallax 0: fixed on screen
}

NNH_TEST(opacity_multiplies_the_layers_and_hidden_or_invisible_layers_are_skipped) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    s.map.set_tile(s.front, 0, 0, tile(1));
    s.map.set_layer_opacity(s.back, 0.5f);
    s.map.set_layer_visible(s.front, false);
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.opacity = 0.5f});
    gfx::draw_tilemap(Screen::Bottom, s.map, {.opacity = 0});
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "t") == 0);
    CHECK(batch(0).color.a == 64 && batch(0).color.r == 255);
}

NNH_TEST(a_tile_set_after_the_draw_call_is_drawn_that_frame) {
    Scene s;
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map, {.map_layer = s.back});
    s.map.set_tile(s.back, 1, 0, tile(4)); // the layer had no tiles when it was drawn: no batch
    gfx::draw_tilemap(Screen::Top, s.map, {.map_layer = s.back});
    s.map.set_tile(s.back, 1, 0, tile(7)); // changed again before the frame ends
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 1 && quad(0).src_x == 32 && quad(0).src_y == 16); // id 7
}

NNH_TEST(a_map_destroyed_after_its_draw_call_draws_nothing) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    s.map.reset();
    gfx::draw(Screen::Top, s.sheet, {0, 0});
    CHECK(nnh::frame());
    CHECK(std::strcmp(calls(), "T") == 0 && tiles().batch_count == 0);
}

NNH_TEST(a_map_moved_after_its_draw_call_is_still_drawn) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    nnh::Tilemap moved = std::move(s.map);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 1);
}

NNH_TEST(stats_and_drops_count_map_tiles) {
    Scene s(100);
    s.map.fill(s.back, 0, 0, 30, 20, tile(1)); // 25 x 15 = 375 visible
    g_scene = &s;
    CHECK(nnh::frame());
    gfx::draw_tilemap(Screen::Top, s.map);
    const bool crashed = fake::run_expecting_crash([] { (void)nnh::frame(); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
    CHECK(nnh::gfx::stats().tiles_last_frame == 100 && nnh::gfx::stats().dropped_tiles == 275);
}

NNH_TEST(drawing_an_unknown_map_layer_asserts) {
    Scene s;
    g_scene = &s;
    CHECK(nnh::frame());
    CHECK(fake::run_expecting_crash(draw_bad_layer) == nnh::build::kAssertsEnabled);
}

NNH_TEST(a_tileset_whose_texture_was_destroyed_is_skipped_and_asserts) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    s.sheet.reset(); // outside a frame: destroyed now
    g_scene = &s;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(draw_whole_map);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "texture") != nullptr);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 0);
}

NNH_TEST(without_max_tiles_drawing_a_map_asserts_with_the_fix) {
    Scene s(0);
    s.map.set_tile(s.back, 0, 0, tile(1));
    g_scene = &s;
    CHECK(nnh::frame());
    const bool crashed = fake::run_expecting_crash(draw_whole_map);
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (crashed) CHECK(std::strstr(fake::last_crash().top, "max_tiles") != nullptr);
}

NNH_TEST(an_empty_map_or_a_call_outside_a_frame_draws_nothing) {
    Scene s;
    s.map.set_tile(s.back, 0, 0, tile(1));
    gfx::draw_tilemap(Screen::Top, s.map); // before the first frame
    CHECK(nnh::frame());
    nnh::Tilemap empty;
    gfx::draw_tilemap(Screen::Top, empty);
    CHECK(nnh::frame());
    CHECK(tiles().batch_count == 0);
}
