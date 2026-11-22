// Tiled tile animations → the map model → `.nnhmap` (spec 012 T6.3): frames in the tile's own tileset, durations,
// the per-animation and per-map limits, and the animation table the writer lays out for the engine.

#include <string>

#include "map_writer.hpp"
#include "test.hpp"
#include "tiled_fixture.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/map_validate.hpp"

using namespace nnh::assetc;
using tiled_fixture::Files;
namespace fmt = nnh::detail::mapfmt;

namespace {

/// An embedded tileset of tiles/world.png (64x32, 8 tiles of 16x16) whose "tiles" are `tiles`.
std::string with_tiles(const std::string& tiles) {
    return R"({"firstgid": 1, "name": "world", "image": "../tiles/world.png", "imagewidth": 64, "imageheight": 32,
               "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4, "tiles": [)" + tiles + "]}";
}

Files with_world() {
    Files f;
    f.add("tiles/world.png", tiled_fixture::png(64, 32));
    return f;
}

bool refused(const std::string& tiles, const char* reason) {
    Files f = with_world();
    Tileset set;
    const bool ok = tiled_fixture::read_tileset(f, with_tiles(tiles), set);
    if (!ok && !f.mentions(reason)) std::printf("    got: %s\n", f.problem().c_str());
    return !ok && f.mentions(reason);
}

/// `count` frames of tile 0, 10 ms each.
std::string frames(int count) {
    std::string list;
    for (int i = 0; i < count; ++i) list += std::string(i ? ", " : "") + R"({"tileid": 0, "duration": 10})";
    return "[" + list + "]";
}

/// A one-cell map with one tileset of `tiles` tiles (16 x 16, a 1024 x 1024 image: up to 4096 fit).
Map map_of(int tiles) {
    Map m;
    m.width = m.height = 1;
    m.tile_width = m.tile_height = 16;
    Tileset s;
    s.first_id = 1;
    s.tile_count = tiles;
    s.columns = 64;
    s.image_width = s.image_height = 1024;
    s.image = "tiles/big";
    m.tilesets.push_back(s);
    return m;
}

} // namespace

NNH_TEST(tileset_reads_tile_animations_with_local_tile_ids) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, with_tiles(R"({"id": 1, "animation": [{"tileid": 0, "duration": 100},
                                                                           {"tileid": 7, "duration": 65535}]})"),
                                      set));
    CHECK(set.tiles.size() == 1 && set.tiles[1].collision == 0 && set.tiles[1].properties.empty());
    const std::vector<AnimationFrame>& a = set.tiles[1].animation;
    CHECK(a.size() == 2 && a[0].tile == 0 && a[0].ms == 100 && a[1].tile == 7 && a[1].ms == 65535);
}

NNH_TEST(tileset_keeps_animations_next_to_collision_and_properties) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, with_tiles(R"({"id": 3, "animation": [{"tileid": 4, "duration": 5}],
        "properties": [{"name": "solid", "type": "bool", "value": true}, {"name": "kind", "value": "lava"}]})"),
                                      set));
    CHECK(set.tiles[3].collision == 1 && set.tiles[3].properties.size() == 1 && set.tiles[3].animation.size() == 1);
}

NNH_TEST(tileset_takes_an_empty_animation_as_none) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, with_tiles(R"({"id": 1, "animation": []})"), set));
    CHECK(set.tiles.empty());
}

NNH_TEST(tileset_reads_32_frames_and_refuses_33) {
    Files f = with_world();
    Tileset set;
    CHECK(tiled_fixture::read_tileset(f, with_tiles(R"({"id": 1, "animation": )" + frames(32) + "}"), set));
    CHECK(set.tiles[1].animation.size() == 32);
    CHECK(refused(R"({"id": 1, "animation": )" + frames(33) + "}", "tile 1: the animation has 33 frames: at most 32"));
}

NNH_TEST(tileset_refuses_animation_frames_outside_the_tileset_or_without_a_duration) {
    CHECK(refused(R"({"id": 1, "animation": [{"tileid": 8, "duration": 10}]})",
                  "tile 1: animation frame 0: \"tileid\" must be a whole number from 0 to 7"));
    CHECK(refused(R"({"id": 1, "animation": [{"tileid": 2, "duration": 10}, {"tileid": -1, "duration": 10}]})",
                  "animation frame 1: \"tileid\""));
    CHECK(refused(R"({"id": 1, "animation": [{"tileid": 2, "duration": 0}]})",
                  "animation frame 0: \"duration\" must be a whole number from 1 to 65535"));
    CHECK(refused(R"({"id": 1, "animation": [{"tileid": 2, "duration": 65536}]})", "\"duration\""));
    CHECK(refused(R"({"id": 1, "animation": [{"tileid": 2}]})", "\"duration\" is missing"));
    CHECK(refused(R"({"id": 1, "animation": [3]})", "animation frame 0 must be an object"));
    CHECK(refused(R"({"id": 1, "animation": {"tileid": 2}})", "\"animation\" must be an array"));
}

NNH_TEST(writer_lays_out_animations_sorted_with_engine_ids) {
    Map m = map_of(8);
    Tileset second = m.tilesets[0];
    second.first_id = 9;
    second.tile_count = 4;
    m.tilesets.push_back(second);
    m.tilesets[0].tiles[5].animation = {{5, 100}, {6, 50}};
    m.tilesets[0].tiles[1].animation = {{2, 30}};
    m.tilesets[0].tiles[1].collision = 1;
    m.tilesets[1].tiles[0].animation = {{3, 70}};
    std::vector<std::uint8_t> out;
    std::string error;
    CHECK(write_map(m, out, error));
    CHECK(nnh::detail::validate_map(out.data(), out.size()).ok());
    const nnh::detail::MapView v{out.data()};
    const fmt::Header& h = v.header();
    CHECK(h.animations == 3 && h.animations_cap == 3 && h.frames == 4 && h.frames_cap == 4);
    const auto* a = v.section<fmt::AnimationEntry>(fmt::kAnimations);
    const auto* f = v.section<fmt::FrameEntry>(fmt::kFrames);
    CHECK(a[0].tile_id == 2 && a[0].frame_count == 1 && a[0].first_frame == 0);
    CHECK(a[1].tile_id == 6 && a[1].frame_count == 2 && a[1].first_frame == 1);
    CHECK(a[2].tile_id == 9 && a[2].frame_count == 1 && a[2].first_frame == 3);
    CHECK(f[0].tile_id == 3 && f[0].ms == 30);
    CHECK(f[1].tile_id == 6 && f[1].ms == 100 && f[2].tile_id == 7 && f[2].ms == 50);
    CHECK(f[3].tile_id == 12 && f[3].ms == 70); // local 3 of the set starting at 9
    CHECK(v.tile_flags()[2] == (fmt::kTileAnimated | 1) && v.tile_flags()[6] == fmt::kTileAnimated);
    CHECK(v.tile_flags()[9] == fmt::kTileAnimated && v.tile_flags()[1] == 0);
}

NNH_TEST(writer_refuses_more_animations_than_a_map_holds) {
    Map m = map_of(300);
    for (int t = 0; t < 257; ++t) m.tilesets[0].tiles[t].animation = {{t, 10}};
    std::vector<std::uint8_t> out;
    std::string error;
    CHECK(!write_map(m, out, error) && error.find("more than 256 animated tiles") != std::string::npos);
    m.tilesets[0].tiles.erase(256);
    CHECK(write_map(m, out, error));
}

NNH_TEST(writer_refuses_more_animation_frames_than_a_map_holds) {
    Map m = map_of(200);
    for (int t = 0; t < 128; ++t) m.tilesets[0].tiles[t].animation.assign(32, AnimationFrame{t, 10});
    std::vector<std::uint8_t> out;
    std::string error;
    CHECK(write_map(m, out, error)); // exactly 4096
    m.tilesets[0].tiles[128].animation = {{0, 10}};
    CHECK(!write_map(m, out, error) && error.find("more than 4096 animation frames") != std::string::npos);
}
