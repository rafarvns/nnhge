// Validation of the animation table of `.nnhmap` files (spec 012 stage 6, FR-1, constitution X): tiles and their
// animated flags, sorted entries, frame runs that follow each other, frame tiles in the animated tile's tileset,
// durations and counts within the limits.

#include <cstring>

#include "map_file.hpp"
#include "map_fixture.hpp"
#include "tilemap/map_validate.hpp"

using map_file::Bytes;
using nnh::detail::MapProblem;
using nnh::detail::validate_map;
namespace fmt = nnh::detail::mapfmt;

namespace {

bool rejects(Bytes& file, MapProblem problem, const char* words) {
    map_file::seal(file);
    const nnh::detail::MapCheck c = validate_map(file.data(), file.size());
    return c.problem == problem && std::strstr(c.reason, words) != nullptr;
}

bool accepts(Bytes& file) {
    map_file::seal(file);
    return validate_map(file.data(), file.size()).ok();
}

fmt::AnimationEntry* animations(Bytes& file) {
    return map_file::view(file).section<fmt::AnimationEntry>(fmt::kAnimations);
}
fmt::FrameEntry* frames(Bytes& file) { return map_file::view(file).section<fmt::FrameEntry>(fmt::kFrames); }

} // namespace

NNH_TEST(map_validate_accepts_animated_tiles) {
    Bytes file = map_file::animated();
    CHECK(validate_map(file.data(), file.size()).ok());
}

NNH_TEST(map_validate_refuses_animations_of_unknown_tiles) {
    Bytes file = map_file::animated();
    animations(file)[1].tile_id = 9;
    CHECK(rejects(file, MapProblem::Damaged, "unknown tile"));
    file = map_file::animated();
    animations(file)[0].tile_id = 0;
    CHECK(rejects(file, MapProblem::Damaged, "unknown tile"));
}

NNH_TEST(map_validate_refuses_an_unsorted_animation_table) {
    Bytes file = map_file::animated();
    animations(file)[0].tile_id = 6;
    animations(file)[1].tile_id = 2;
    CHECK(rejects(file, MapProblem::Damaged, "sorted"));
    file = map_file::animated();
    animations(file)[1].tile_id = 2; // twice the same tile
    CHECK(rejects(file, MapProblem::Damaged, "sorted"));
}

NNH_TEST(map_validate_refuses_animated_flags_that_do_not_match_the_table) {
    Bytes file = map_file::animated();
    map_file::view(file).tile_flags()[3] |= fmt::kTileAnimated; // a flag without an animation
    CHECK(rejects(file, MapProblem::Damaged, "animated flags"));
    file = map_file::animated();
    map_file::view(file).tile_flags()[6] &= static_cast<std::uint8_t>(~fmt::kTileAnimated); // and the reverse
    CHECK(rejects(file, MapProblem::Damaged, "animated flags"));
    file = map_file::animated();
    animations(file)[1].tile_id = 7; // as many flags as animations, but on another tile
    CHECK(rejects(file, MapProblem::Damaged, "animated flags"));
    file = map_file::sample();
    map_file::view(file).tile_flags()[2] |= fmt::kTileAnimated; // no table at all
    CHECK(rejects(file, MapProblem::Damaged, "animated flags"));
}

NNH_TEST(map_validate_refuses_animations_without_frames_or_over_32) {
    Bytes file = map_file::animated();
    animations(file)[1].frame_count = 0;
    CHECK(rejects(file, MapProblem::Damaged, "without frames"));
    file = map_file::animated();
    animations(file)[1].frame_count = 33;
    CHECK(rejects(file, MapProblem::OverLimit, "32 frames"));
}

NNH_TEST(map_validate_refuses_frame_runs_that_do_not_follow_each_other) {
    Bytes file = map_file::animated();
    animations(file)[1].first_frame = 1; // overlaps the first animation's frames
    CHECK(rejects(file, MapProblem::Damaged, "follow"));
    file = map_file::animated();
    animations(file)[0].first_frame = 1;
    CHECK(rejects(file, MapProblem::Damaged, "follow"));
    file = map_file::animated();
    animations(file)[1].frame_count = 2; // runs past the last frame
    CHECK(rejects(file, MapProblem::Damaged, "follow"));
}

NNH_TEST(map_validate_refuses_frames_that_do_not_add_up_to_the_header) {
    Bytes file = map_file::animated();
    animations(file)[1].frame_count = 0;
    map_file::view(file).header().animations = 1; // one animation of 2 frames, the header says 3
    map_file::view(file).tile_flags()[6] = 0;
    CHECK(rejects(file, MapProblem::Damaged, "add up"));
    file = map_file::animated();
    map_file::view(file).header().animations = 0; // frames without animations
    map_file::view(file).tile_flags()[2] = 0;
    map_file::view(file).tile_flags()[6] = 0;
    CHECK(rejects(file, MapProblem::Damaged, "add up"));
}

NNH_TEST(map_validate_refuses_frames_of_unknown_tiles_or_0_ms) {
    Bytes file = map_file::animated();
    frames(file)[2].tile_id = 9;
    CHECK(rejects(file, MapProblem::Damaged, "frame"));
    file = map_file::animated();
    frames(file)[0].tile_id = 0;
    CHECK(rejects(file, MapProblem::Damaged, "frame"));
    file = map_file::animated();
    frames(file)[1].ms = 0;
    CHECK(rejects(file, MapProblem::Damaged, "0 ms"));
    file = map_file::animated();
    frames(file)[2].ms = 65535; // the longest frame is fine
    CHECK(accepts(file));
}

NNH_TEST(map_validate_refuses_frames_from_another_tileset) {
    map_test::Engine engine;
    nnh::Texture single = gfx_test::load("tile");
    {
        nnh::Tilemap map = map_test::make({.width = 2, .height = 2, .max_animations = 2, .max_animation_frames = 2});
        CHECK(map_test::tileset(map, {.texture = &engine.sheet}) == 1);
        CHECK(map_test::tileset(map, {.texture = &single}) == 9);
        const nnh::Tilemap::AnimationFrame one[] = {{.tile_id = 9, .ms = 10}};
        const nnh::Tilemap::AnimationFrame two[] = {{.tile_id = 8, .ms = 10}};
        CHECK(map.set_tile_animation(9, one, 1).ok());
        CHECK(map.set_tile_animation(1, two, 1).ok());
        Bytes file = map_file::from_map(map);
        CHECK(accepts(file));
        frames(file)[0].tile_id = 9; // tile 1 (first set) animated by tile 9 (second set)
        CHECK(rejects(file, MapProblem::Damaged, "tileset"));
        file = map_file::from_map(map);
        frames(file)[1].tile_id = 8; // and the reverse
        CHECK(rejects(file, MapProblem::Damaged, "tileset"));
    }
    single.reset();
}
