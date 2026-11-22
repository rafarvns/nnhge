// Tiled tilesets → the map model (spec 012 T5.2): embedded and .tsj tilesets, images inside the assets folder, PNG
// sizes, margin and spacing, solid/one_way, tile properties, and what is refused.

#include <string>

#include "test.hpp"
#include "tiled_fixture.hpp"

using namespace nnh::assetc;
using tiled_fixture::Files;

namespace {

/// An embedded tileset of tiles/world.png (64x32, 16x16 tiles) with `extra` members.
std::string embedded(const std::string& extra = "") {
    return R"({"firstgid": 1, "name": "world", "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
               "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4)" + extra + "}";
}

Files with_world() {
    Files f;
    f.add("tiles/world.png", tiled_fixture::png(64, 32));
    return f;
}

bool refused(Files& f, const std::string& entry, const char* reason) {
    Tileset set;
    const bool ok = tiled_fixture::read_tileset(f, entry, set);
    if (!ok && !f.mentions(reason)) std::printf("    got: %s\n", f.problem().c_str());
    return !ok && f.mentions(reason);
}

} // namespace

NNH_TEST(tileset_embedded_reads_image_and_grid) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, embedded(), set));
    CHECK(set.first_gid == 1 && set.tile_count == 8 && set.columns == 4);
    CHECK(set.image == "tiles/world");
    CHECK(set.image_width == 64 && set.image_height == 32);
    CHECK(set.margin == 0 && set.spacing == 0 && set.tiles.empty());
    CHECK(f.ctx.dependencies.size() == 1 && f.ctx.dependencies[0] == "/game/assets/tiles/world.png");
}

NNH_TEST(tileset_external_tsj_is_read_relative_to_the_map_and_its_image_to_it) {
    Files f;
    f.add("art/sets/world.tsj", R"({"type": "tileset", "image": "../img/w.png", "imagewidth": 64, "imageheight": 64,
                                     "tilewidth": 16, "tileheight": 16, "tilecount": 9, "columns": 3,
                                     "margin": 1, "spacing": 4})");
    f.add("art/img/w.png", tiled_fixture::png(64, 64));
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, R"({"firstgid": 5, "source": "../art/sets/world.tsj"})", set));
    CHECK(set.first_gid == 5 && set.tile_count == 9 && set.columns == 3 && set.margin == 1 && set.spacing == 4);
    CHECK(set.image == "art/img/w");
    CHECK(f.ctx.dependencies.size() == 2);
    CHECK(f.ctx.dependencies[0] == "/game/assets/art/sets/world.tsj");
    CHECK(f.ctx.dependencies[1] == "/game/assets/art/img/w.png");
}

NNH_TEST(tileset_solid_and_one_way_become_collision_not_properties) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, embedded(R"(, "tiles": [
        {"id": 2, "properties": [{"name": "solid", "type": "bool", "value": true}]},
        {"id": 3, "properties": [{"name": "one_way", "type": "bool", "value": true},
                                 {"name": "damage", "type": "int", "value": 3}]},
        {"id": 4, "properties": [{"name": "solid", "type": "bool", "value": false}]}])"), set));
    CHECK(set.tiles.size() == 2);
    CHECK(set.tiles[2].collision == 1 && set.tiles[2].properties.empty());
    CHECK(set.tiles[3].collision == 2 && set.tiles[3].properties.size() == 1);
    CHECK(set.tiles[3].properties[0].name == "damage" && set.tiles[3].properties[0].integer == 3);
}

NNH_TEST(tileset_ignores_editor_only_tile_data) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, embedded(R"(, "tiles": [{"id": 1, "probability": 0.5, "type": "x",
        "objectgroup": {"objects": []}}], "transformations": {"hflip": true}, "objectalignment": "unspecified")"),
                                      set));
    CHECK(set.tiles.empty());
}

NNH_TEST(tileset_refuses_xml_tilesets) {
    Files f = with_world();
    CHECK(refused(f, R"({"firstgid": 1, "source": "../tiles/world.tsx"})", "export the tileset as JSON (.tsj)"));
    CHECK(f.ctx.problem.file == "/game/assets/levels/map.tmj");
}

NNH_TEST(tileset_refuses_a_missing_or_broken_tsj) {
    Files f = with_world();
    CHECK(refused(f, R"({"firstgid": 1, "source": "../tiles/none.tsj"})", "tileset ../tiles/none.tsj not found"));
    f.add("tiles/broken.tsj", "{\"image\": ");
    CHECK(refused(f, R"({"firstgid": 1, "source": "../tiles/broken.tsj"})", "line 1, column 11"));
    CHECK(f.ctx.problem.file == "/game/assets/tiles/broken.tsj");
}

NNH_TEST(tileset_refuses_image_collections) {
    Files f = with_world();
    CHECK(refused(f, R"({"firstgid": 1, "tilewidth": 16, "tileheight": 16, "tilecount": 1, "columns": 0,
                         "tiles": [{"id": 0, "image": "a.png", "imagewidth": 16, "imageheight": 16}]})",
                  "image collection"));
}

NNH_TEST(tileset_refuses_wang_sets) {
    Files f = with_world();
    CHECK(refused(f, embedded(R"(, "wangsets": [{"name": "grass"}])"), "wang sets"));
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, embedded(R"(, "wangsets": [])"), set)); // an empty list is fine
}

NNH_TEST(tileset_refuses_another_tile_size) {
    Files f = with_world();
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 32, "tileheight": 32, "tilecount": 2, "columns": 2})",
                  "tiles are 32x32 but the map's are 16x16"));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 16, "tileheight": 32, "tilecount": 4, "columns": 4})",
                  "tiles are 16x32 but the map's are 16x16"));
}

NNH_TEST(tileset_refuses_images_outside_the_assets_or_not_png) {
    Files f = with_world();
    f.memory.add("/game/outside.png", tiled_fixture::png(64, 32));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../../outside.png", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4})",
                  "../../outside.png is outside the assets folder"));
    f.add("tiles/world.jpg", tiled_fixture::png(64, 32));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/world.jpg", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4})",
                  "must be a .png"));
}

NNH_TEST(tileset_refuses_missing_and_non_png_images) {
    Files f;
    CHECK(refused(f, embedded(), "tileset image tiles/world.png not found"));
    f.add("tiles/world.png", "GIF89a, not a png at all.....");
    CHECK(refused(f, embedded(), "tiles/world.png: not a PNG file"));
}

NNH_TEST(tileset_refuses_an_image_of_another_size_than_tiled_says) {
    Files f;
    f.add("tiles/world.png", tiled_fixture::png(128, 32));
    CHECK(refused(f, embedded(), "tiles/world.png is 128x32 but the tileset says 64x32"));
    f.add("tiles/world.png", tiled_fixture::png(64, 64));
    CHECK(refused(f, embedded(), "tiles/world.png is 64x64 but the tileset says 64x32"));
}

NNH_TEST(tileset_refuses_images_that_are_not_textures) {
    Files f;
    f.add("tiles/odd.png", tiled_fixture::png(100, 60));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/odd.png", "imagewidth": 100, "imageheight": 60,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 18, "columns": 6})",
                  "tiles/odd.png is 100x60: tileset images need power-of-two sides from 8 to 1024"));
    f.add("tiles/big.png", tiled_fixture::png(2048, 16));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/big.png", "imagewidth": 2048, "imageheight": 16,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 128, "columns": 128})",
                  "power-of-two sides from 8 to 1024"));
    f.add("tiles/thin.png", tiled_fixture::png(4, 64));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/thin.png", "imagewidth": 4, "imageheight": 64,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 1, "columns": 0})",
                  "tiles/thin.png is 4x64: tileset images need power-of-two sides from 8 to 1024"));
}

NNH_TEST(tileset_refuses_a_grid_that_does_not_match_the_image) {
    Files f = with_world();
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 3})",
                  "3 columns, but 4 tiles of 16 px fit across the image"));
    CHECK(refused(f, R"({"firstgid": 1, "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
                         "tilewidth": 16, "tileheight": 16, "tilecount": 9, "columns": 4})",
                  "9 tiles, but only 8 fit in the image"));
}

NNH_TEST(tileset_refuses_bad_tile_entries) {
    Files f = with_world();
    CHECK(refused(f, embedded(R"(, "tiles": [{"id": 8}])"), "\"id\" must be a whole number from 0 to 7"));
    CHECK(refused(f, embedded(R"(, "tiles": [{"id": 1, "properties": [{"name": "solid", "type": "int", "value": 1}]}])"),
                  "\"solid\" must be a bool"));
    CHECK(refused(f, embedded(R"(, "tiles": [{"id": 1, "properties": [{"name": "solid", "type": "bool", "value": true},
                                             {"name": "one_way", "type": "bool", "value": true}]}])"),
                  "both solid and one_way"));
    CHECK(refused(f, embedded(R"(, "tiles": [{"id": 2, "properties": [{"name": "a", "value": "x"}]},
                                             {"id": 2, "properties": [{"name": "b", "value": "y"}]}])"),
                  "tile 2 is listed twice"));
}

NNH_TEST(tileset_refuses_tile_offsets_and_bad_numbers) {
    Files f = with_world();
    CHECK(refused(f, embedded(R"(, "tileoffset": {"x": 0, "y": 4})"), "tile offset"));
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, embedded(R"(, "tileoffset": {"x": 0, "y": 0})"), set));
    CHECK(refused(f, embedded(R"(, "margin": -1)"), "\"margin\" must be a whole number from 0"));
    CHECK(refused(f, R"({"firstgid": 0, "image": "../tiles/world.png"})", "\"firstgid\""));
}
