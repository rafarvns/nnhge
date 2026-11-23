// Building maps in code (spec 012 FR-0, FR-11): create, capacities, limits, move-only ownership, memory.

#include <cstring>
#include <utility>

#include "map_fixture.hpp"
#include "nnh/memory.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/map_runtime.hpp"
#include "tilemap/tilemap_internal.hpp"

using map_test::make;
using nnh::ErrorCode;
using nnh::Tilemap;
using nnh::TilemapDesc;

namespace {

std::size_t map_bytes() { return nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).bytes; }
std::size_t map_blocks() { return nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count; }

ErrorCode create_error(const TilemapDesc& desc) {
    auto r = Tilemap::create(desc);
    return r ? ErrorCode::None : r.error().code;
}

} // namespace

NNH_TEST(a_map_is_created_with_its_size_and_tile_size) {
    Tilemap map = make({.width = 40, .height = 15, .tile_width = 16, .tile_height = 8});
    CHECK(map.valid());
    CHECK(map.width() == 40 && map.height() == 15);
    CHECK(map.tile_width() == 16 && map.tile_height() == 8);
    CHECK(map.tileset_count() == 0 && map.tile_id_count() == 0);
}

NNH_TEST(a_map_is_one_block_under_the_tilemap_tag_sized_by_the_formula) {
    const std::size_t before = map_bytes();
    {
        Tilemap map = make({.width = 40, .height = 15});
        CHECK(map_blocks() == 1);
        CHECK(map_bytes() - before == 13100); // the default capacities, by the formula in docs/api/tilemap.md
        const TilemapDesc desc{.width = 40, .height = 15};
        CHECK(map_bytes() - before == nnh::detail::map_block_bytes(nnh::detail::capacities_for(desc)));
    }
    CHECK(map_bytes() == before);
    CHECK(map_blocks() == 0);
}

NNH_TEST(the_memory_examples_in_the_docs_are_exact) {
    const auto bytes = [](const TilemapDesc& d) { return nnh::detail::map_block_bytes(nnh::detail::capacities_for(d)); };
    CHECK(bytes({.width = 240, .height = 30, .max_tile_layers = 3, .max_tilesets = 1, .max_tile_ids = 128,
                 .max_object_layers = 1, .max_objects = 40, .max_properties = 60, .string_bytes = 1024}) ==
          47676 + 132 + 4 * 129);
    CHECK(bytes({.width = 512, .height = 512, .max_tile_layers = 4, .max_tilesets = 2, .max_tile_ids = 1024}) ==
          2108132 + 132 + 4 * 1025);
}

NNH_TEST(a_map_is_freed_by_reset_and_becomes_empty) {
    Tilemap map = make({.width = 4, .height = 4});
    map.reset();
    CHECK(!map.valid());
    CHECK(map_blocks() == 0);
    map.reset(); // twice is harmless
    CHECK(map.width() == 0);
}

NNH_TEST(moving_a_map_moves_its_block) {
    Tilemap a = make({.width = 4, .height = 4});
    Tilemap b = std::move(a);
    CHECK(!a.valid() && b.valid() && b.width() == 4);
    Tilemap c = make({.width = 8, .height = 2});
    CHECK(map_blocks() == 2);
    c = std::move(b); // c's old block is freed
    CHECK(c.valid() && c.width() == 4 && map_blocks() == 1);
    c = std::move(c); // self-move keeps it
    CHECK(c.valid() && map_blocks() == 1);
}

NNH_TEST(an_empty_map_answers_queries_with_empty_values) {
    Tilemap map;
    CHECK(!map.valid() && map.width() == 0 && map.height() == 0 && map.tile_width() == 0);
    CHECK(map.tile_height() == 0 && map.tileset_count() == 0 && map.tile_id_count() == 0);
}

NNH_TEST(map_sizes_below_one_are_invalid) {
    CHECK(create_error({.width = 0, .height = 4}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.width = 4, .height = -1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.width = 4, .height = 4, .max_objects = -1}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.width = 4, .height = 4, .string_bytes = -1}) == ErrorCode::InvalidArgument);
    CHECK(map_blocks() == 0);
}

NNH_TEST(tile_sizes_outside_8_to_64_are_invalid) {
    CHECK(create_error({.width = 4, .height = 4, .tile_width = 7}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.width = 4, .height = 4, .tile_height = 65}) == ErrorCode::InvalidArgument);
    CHECK(create_error({.width = 4, .height = 4, .tile_width = 8, .tile_height = 64}) == ErrorCode::None);
}

NNH_TEST(capacities_over_the_engine_limits_are_refused) {
    CHECK(create_error({.width = 1025, .height = 1}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .max_tile_layers = 17}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .max_tilesets = 9}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .max_tile_ids = 8192}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .max_objects = 4097}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .max_properties = 16385}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 4, .height = 4, .string_bytes = 65537}) == ErrorCode::LimitReached);
    CHECK(create_error({.width = 1024, .height = 1024, .max_tile_layers = 5}) == ErrorCode::LimitReached);
    CHECK(map_blocks() == 0);
}

NNH_TEST(the_platform_running_out_of_memory_fails_create) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 1024);
    CHECK(create_error({.width = 64, .height = 64}) == ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

// --- tilesets (T2.3) ---

namespace {

nnh::detail::MapView view_of(const Tilemap& map) { return nnh::detail::map_view(map); }
const nnh::detail::mapfmt::TileSource& source(const Tilemap& map, int id) { return view_of(map).tile_sources()[id]; }
ErrorCode tileset_error(Tilemap& map, const nnh::TilesetDesc& desc) {
    auto r = map.add_tileset(desc);
    return r ? ErrorCode::None : r.error().code;
}
ErrorCode layer_error(Tilemap& map, const nnh::TileLayerDesc& desc) {
    auto r = map.add_tile_layer(desc);
    return r ? ErrorCode::None : r.error().code;
}

} // namespace

NNH_TEST(a_tileset_gets_ids_for_every_tile_that_fits) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    CHECK(map_test::tileset(map, {.texture = &engine.sheet}) == 1); // 64x32 -> 4 x 2 tiles of 16x16
    CHECK(map.tileset_count() == 1 && map.tile_id_count() == 8);
    CHECK(source(map, 1).x == 0 && source(map, 1).y == 0);
    CHECK(source(map, 4).x == 48 && source(map, 4).y == 0);
    CHECK(source(map, 5).x == 0 && source(map, 5).y == 16);
    CHECK(source(map, 8).x == 48 && source(map, 8).y == 16);
}

NNH_TEST(a_second_tileset_starts_after_the_first) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    CHECK(map_test::tileset(map, {.texture = &engine.sheet, .tile_count = 3}) == 1);
    CHECK(map_test::tileset(map, {.texture = &engine.sheet}) == 4);
    CHECK(map.tile_id_count() == 11);
    CHECK(source(map, 4).x == 0 && source(map, 11).x == 48 && source(map, 11).y == 16);
    const auto& second = view_of(map).tilesets()[1];
    CHECK(second.first_id == 4 && second.tile_count == 8 && second.columns == 4);
    CHECK(second.image_width == 64 && second.image_height == 32);
}

NNH_TEST(a_tileset_with_margin_and_spacing_skips_them) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4, .tile_width = 8, .tile_height = 8});
    // (64 - 2*1 + 1) / (8 + 1) = 7 columns, (32 - 2 + 1) / 9 = 3 rows
    CHECK(map_test::tileset(map, {.texture = &engine.sheet, .margin = 1, .spacing = 1}) == 1);
    CHECK(map.tile_id_count() == 21);
    CHECK(source(map, 1).x == 1 && source(map, 1).y == 1);
    CHECK(source(map, 2).x == 10 && source(map, 7).x == 55);
    CHECK(source(map, 8).x == 1 && source(map, 8).y == 10);
    CHECK(source(map, 21).x == 55 && source(map, 21).y == 19);
}

NNH_TEST(a_tileset_with_non_square_tiles_uses_the_map_tile_size) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4, .tile_width = 32, .tile_height = 8});
    CHECK(map_test::tileset(map, {.texture = &engine.sheet}) == 1); // 2 columns x 4 rows
    CHECK(map.tile_id_count() == 8);
    CHECK(source(map, 2).x == 32 && source(map, 3).x == 0 && source(map, 3).y == 8);
}

NNH_TEST(a_tileset_needs_a_valid_texture) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    CHECK(tileset_error(map, {}) == ErrorCode::InvalidArgument);
    nnh::Texture none;
    CHECK(tileset_error(map, {.texture = &none}) == ErrorCode::InvalidArgument);
    CHECK(map.tileset_count() == 0);
}

NNH_TEST(a_tileset_with_bad_numbers_is_invalid) {
    map_test::Engine engine;
    Tilemap big = make({.width = 4, .height = 4, .tile_width = 64, .tile_height = 64});
    CHECK(tileset_error(big, {.texture = &engine.sheet}) == ErrorCode::InvalidArgument); // no tile fits
    Tilemap map = make({.width = 4, .height = 4});
    CHECK(tileset_error(map, {.texture = &engine.sheet, .tile_count = 9}) == ErrorCode::InvalidArgument);
    CHECK(tileset_error(map, {.texture = &engine.sheet, .margin = -1}) == ErrorCode::InvalidArgument);
    CHECK(tileset_error(map, {.texture = &engine.sheet, .spacing = -1}) == ErrorCode::InvalidArgument);
    CHECK(tileset_error(map, {.texture = &engine.sheet, .tile_count = -1}) == ErrorCode::InvalidArgument);
    CHECK(map.tileset_count() == 0 && map.tile_id_count() == 0);
}

NNH_TEST(tilesets_beyond_the_capacities_are_refused_and_nothing_grows) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4, .max_tilesets = 2, .max_tile_ids = 12});
    CHECK(map_test::tileset(map, {.texture = &engine.sheet}) == 1);
    CHECK(tileset_error(map, {.texture = &engine.sheet}) == ErrorCode::LimitReached); // 8 + 8 > 12 ids
    CHECK(map_test::tileset(map, {.texture = &engine.sheet, .tile_count = 4}) == 9);
    CHECK(tileset_error(map, {.texture = &engine.sheet, .tile_count = 1}) == ErrorCode::LimitReached);
    CHECK(map.tileset_count() == 2 && map.tile_id_count() == 12);
    CHECK(map_blocks() == 1);
}

NNH_TEST(a_tileset_remembers_its_texture_handle_and_name) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    map_test::tileset(map, {.texture = &engine.sheet, .name = "world"});
    CHECK(std::strcmp(view_of(map).string(view_of(map).tilesets()[0].name), "world") == 0);
    CHECK(nnh::detail::tileset_texture_alive(map, 0));
    CHECK(!nnh::detail::tileset_texture_alive(map, 1)); // no such tileset
}

// --- tile layers (T2.3) ---

NNH_TEST(tile_layers_are_added_in_order_with_their_settings) {
    Tilemap map = make({.width = 4, .height = 3});
    CHECK(map.tile_layer_count() == 0);
    CHECK(map_test::layer(map, {.name = "back", .offset = {2, 3}, .parallax = {0.5f, 0.25f}, .opacity = 0.5f}) == 0);
    CHECK(map_test::layer(map, {.name = "ground", .visible = false, .collision = true}) == 1);
    CHECK(map.tile_layer_count() == 2);
    CHECK(std::strcmp(map.tile_layer_name(0), "back") == 0 && std::strcmp(map.tile_layer_name(1), "ground") == 0);
    CHECK(map.layer_offset(0) == nnh::Vec2(2, 3) && map.layer_parallax(0) == nnh::Vec2(0.5f, 0.25f));
    CHECK(map.layer_opacity(0) == 0.5f && map.layer_visible(0) && !map.layer_collision(0));
    CHECK(!map.layer_visible(1) && map.layer_collision(1) && map.layer_parallax(1) == nnh::Vec2(1, 1));
    CHECK(map.tile(1, 3, 2).empty()); // starts empty
}

NNH_TEST(tile_layers_are_found_by_name) {
    Tilemap map = make({.width = 4, .height = 3});
    map_test::layer(map, {.name = "back"});
    map_test::layer(map, {.name = "front"});
    map_test::layer(map, {.name = "front"});
    CHECK(map.find_tile_layer("front") == 1);
    CHECK(map.find_tile_layer("back") == 0);
    CHECK(map.find_tile_layer("nope") == -1 && map.find_tile_layer(nullptr) == -1);
    CHECK(std::strcmp(map.tile_layer_name(3), "") == 0 && std::strcmp(map.tile_layer_name(-1), "") == 0);
}

NNH_TEST(layer_settings_change_after_creation) {
    Tilemap map = make({.width = 4, .height = 3});
    const int l = map_test::layer(map);
    map.set_layer_offset(l, {-4, 8});
    map.set_layer_parallax(l, {0, 2});
    map.set_layer_opacity(l, 1.5f);
    map.set_layer_visible(l, false);
    map.set_layer_collision(l, true);
    CHECK(map.layer_offset(l) == nnh::Vec2(-4, 8) && map.layer_parallax(l) == nnh::Vec2(0, 2));
    CHECK(map.layer_opacity(l) == 1.0f && !map.layer_visible(l) && map.layer_collision(l));
    map.set_layer_opacity(l, -1);
    CHECK(map.layer_opacity(l) == 0.0f);
    map.set_layer_opacity(9, 0.5f); // invalid layer: ignored
    CHECK(map.layer_opacity(9) == 0.0f && map.layer_offset(9) == nnh::Vec2(0, 0) && !map.layer_visible(9));
}

NNH_TEST(tile_layers_beyond_the_capacity_are_refused) {
    Tilemap map = make({.width = 4, .height = 3, .max_tile_layers = 2});
    map_test::layer(map);
    map_test::layer(map);
    CHECK(layer_error(map, {}) == ErrorCode::LimitReached);
    CHECK(map.tile_layer_count() == 2);
}

NNH_TEST(names_need_room_in_the_strings_and_at_most_255_bytes) {
    Tilemap map = make({.width = 2, .height = 2, .string_bytes = 8});
    CHECK(map_test::layer(map, {.name = "abcdef"}) == 0); // 1 + 7 bytes: full
    CHECK(layer_error(map, {.name = "x"}) == ErrorCode::LimitReached);
    CHECK(map_test::layer(map, {.name = "abcdef"}) == 1); // equal names are shared
    CHECK(map_test::layer(map, {}) == 2);                // "" needs no room
    char long_name[257];
    std::memset(long_name, 'a', 256);
    long_name[256] = '\0';
    Tilemap roomy = make({.width = 2, .height = 2});
    CHECK(layer_error(roomy, {.name = long_name}) == ErrorCode::InvalidArgument);
    long_name[255] = '\0';
    CHECK(layer_error(roomy, {.name = long_name}) == ErrorCode::None);
}

NNH_TEST(a_map_with_no_string_room_still_has_the_empty_string) {
    Tilemap map = make({.width = 2, .height = 2, .max_tile_layers = 1, .string_bytes = 0});
    CHECK(layer_error(map, {}) == ErrorCode::None);
    CHECK(std::strcmp(map.tile_layer_name(0), "") == 0);
    CHECK(layer_error(map, {}) == ErrorCode::LimitReached);
}

NNH_TEST(an_empty_map_refuses_to_build) {
    map_test::Engine engine;
    Tilemap map;
    CHECK(layer_error(map, {}) == ErrorCode::InvalidArgument);
    CHECK(tileset_error(map, {.texture = &engine.sheet}) == ErrorCode::InvalidArgument);
    CHECK(map.tile_layer_count() == 0 && map.find_tile_layer("") == -1);
    CHECK(std::strcmp(map.tile_layer_name(0), "") == 0);
}
