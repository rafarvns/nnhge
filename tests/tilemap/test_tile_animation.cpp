// Animated tiles, pure part (tilemap/tile_animation.hpp; spec 012 stage 6, FR-2, FR-10): the default clock, the
// current frame, lookups in the sorted animation table, and the table a tile pass reads.

#include <cstdint>
#include <vector>

#include "test.hpp"
#include "tilemap/tile_animation.hpp"

namespace fmt = nnh::detail::mapfmt;
using nnh::detail::animation_frame_index;
using nnh::detail::animations_in;
using nnh::detail::default_animation_ms;
using nnh::detail::find_animation;

namespace {

// Three frames: 100 ms, 50 ms, 250 ms (400 ms in all).
constexpr fmt::FrameEntry kFrames[] = {{5, 100}, {6, 50}, {7, 250}};

// Animations of tiles 2, 4 and 9 (sorted by tile id), frames in order.
constexpr fmt::AnimationEntry kTable[] = {
    {.tile_id = 2, .frame_count = 1, .first_frame = 0},
    {.tile_id = 4, .frame_count = 2, .first_frame = 1},
    {.tile_id = 9, .frame_count = 1, .first_frame = 3},
};

} // namespace

NNH_TEST(the_default_animation_clock_is_frames_at_60_hz_in_ms) {
    CHECK(default_animation_ms(0) == 0);
    CHECK(default_animation_ms(1) == 16); // 16.67 rounded down
    CHECK(default_animation_ms(3) == 50);
    CHECK(default_animation_ms(60) == 1000);
    CHECK(default_animation_ms(216000) == 3600000); // an hour, exact
}

NNH_TEST(an_animation_shows_each_frame_for_its_duration) {
    CHECK(animation_frame_index(kFrames, 3, 0) == 0);
    CHECK(animation_frame_index(kFrames, 3, 99) == 0);
    CHECK(animation_frame_index(kFrames, 3, 100) == 1);
    CHECK(animation_frame_index(kFrames, 3, 149) == 1);
    CHECK(animation_frame_index(kFrames, 3, 150) == 2);
    CHECK(animation_frame_index(kFrames, 3, 399) == 2);
}

NNH_TEST(an_animation_loops_over_its_total_duration) {
    CHECK(animation_frame_index(kFrames, 3, 400) == 0);
    CHECK(animation_frame_index(kFrames, 3, 400 + 120) == 1);
    CHECK(animation_frame_index(kFrames, 3, 400ull * 1000000 + 399) == 2);
    CHECK(animation_frame_index(kFrames, 1, 12345) == 0); // one frame: always it
}

NNH_TEST(find_animation_looks_up_a_tile_in_the_sorted_table) {
    CHECK(find_animation(kTable, 3, 2) == 0);
    CHECK(find_animation(kTable, 3, 4) == 1);
    CHECK(find_animation(kTable, 3, 9) == 2);
    CHECK(find_animation(kTable, 3, 3) == -1);
    CHECK(find_animation(kTable, 3, 1) == -1);
    CHECK(find_animation(kTable, 3, 10) == -1);
    CHECK(find_animation(kTable, 0, 2) == -1);
    CHECK(find_animation(kTable + 1, 2, 9) == 1); // within a sub-range
}

NNH_TEST(animations_in_finds_the_run_of_one_tileset) {
    const auto all = animations_in(kTable, 3, 1, 9); // ids 1..9
    CHECK(all.first == 0 && all.count == 3);
    const auto middle = animations_in(kTable, 3, 3, 5); // ids 3..7: tile 4
    CHECK(middle.first == 1 && middle.count == 1);
    const auto last = animations_in(kTable, 3, 9, 1); // id 9 only
    CHECK(last.first == 2 && last.count == 1);
    const auto first = animations_in(kTable, 3, 1, 2); // ids 1..2
    CHECK(first.first == 0 && first.count == 1);
    CHECK(animations_in(kTable, 3, 10, 4).count == 0);
    CHECK(animations_in(kTable, 3, 5, 4).count == 0); // ids 5..8: none
    CHECK(animations_in(kTable, 0, 1, 9).count == 0);
}

namespace {

/// Two tilesets (ids 1..8 and 9..10, 16 x 16 tiles); tile 2 animated by frames 3 (100 ms) and 4 (100 ms), tile 9
/// by frame 10 (50 ms).
struct AnimatedBlock {
    std::vector<std::uint8_t> bytes;
    nnh::detail::MapView v;

    explicit AnimatedBlock(bool animated = true) {
        const nnh::detail::MapCapacities caps{.width = 2, .height = 2, .tilesets = 2, .tile_ids = 10,
                                              .animations = 2, .animation_frames = 3, .tile_layers = 1,
                                              .string_bytes = 4};
        bytes.assign(nnh::detail::map_layout(caps).total, 0);
        nnh::detail::init_map_block(bytes.data(), caps, 16, 16);
        v = {bytes.data()};
        v.tilesets()[0] = {.first_id = 1, .tile_count = 8, .columns = 4, .image_width = 64, .image_height = 32};
        v.tilesets()[1] = {.first_id = 9, .tile_count = 2, .columns = 2, .image_width = 32, .image_height = 16};
        for (int s = 0; s < 2; ++s) {
            const fmt::TilesetEntry& set = v.tilesets()[s];
            for (int i = 0; i < set.tile_count; ++i) {
                v.tile_sources()[set.first_id + i] = nnh::detail::tile_source(set, i, 16, 16);
            }
        }
        v.header().tilesets = 2;
        v.header().tile_ids = 10;
        if (!animated) return;
        auto* anims = v.section<fmt::AnimationEntry>(fmt::kAnimations);
        auto* frames = v.section<fmt::FrameEntry>(fmt::kFrames);
        anims[0] = {.tile_id = 2, .frame_count = 2, .first_frame = 0};
        anims[1] = {.tile_id = 9, .frame_count = 1, .first_frame = 2};
        frames[0] = {3, 100};
        frames[1] = {4, 100};
        frames[2] = {10, 50};
        v.tile_flags()[2] = fmt::kTileAnimated;
        v.tile_flags()[9] = fmt::kTileAnimated;
        v.header().animations = 2;
        v.header().frames = 3;
    }
};

} // namespace

NNH_TEST(prepare_animations_resolves_the_current_frames_of_one_tileset) {
    AnimatedBlock b;
    fmt::TileSource table[fmt::kMaxAnimations] = {};
    nnh::detail::ExpandInput in;
    nnh::detail::prepare_animations(b.v, 0, 150, table, in); // tile 2 shows tile 4 at 150 ms
    CHECK(in.flags == b.v.tile_flags());
    CHECK(in.animation_count == 1 && in.animations == b.v.section<fmt::AnimationEntry>(fmt::kAnimations));
    CHECK(in.animated == table);
    CHECK(table[0].x == 48 && table[0].y == 0); // tile 4: fourth column
    nnh::detail::prepare_animations(b.v, 0, 50, table, in);
    CHECK(table[0].x == 32 && table[0].y == 0); // tile 3
}

NNH_TEST(prepare_animations_takes_only_the_animations_of_its_tileset) {
    AnimatedBlock b;
    fmt::TileSource table[fmt::kMaxAnimations] = {};
    nnh::detail::ExpandInput in;
    nnh::detail::prepare_animations(b.v, 1, 0, table, in);
    CHECK(in.animation_count == 1 && in.animations == b.v.section<fmt::AnimationEntry>(fmt::kAnimations) + 1);
    CHECK(table[0].x == 16 && table[0].y == 0); // tile 10: second tile of the second set
}

NNH_TEST(prepare_animations_leaves_a_pass_without_animations_alone) {
    AnimatedBlock none(false);
    fmt::TileSource table[fmt::kMaxAnimations] = {};
    nnh::detail::ExpandInput in;
    nnh::detail::prepare_animations(none.v, 0, 0, table, in);
    CHECK(in.flags == nullptr && in.animation_count == 0);
    AnimatedBlock b; // animations, but none in a tileset whose ids are 1..2
    b.v.section<fmt::AnimationEntry>(fmt::kAnimations)[0].tile_id = 3;
    b.v.tilesets()[0].tile_count = 2;
    nnh::detail::prepare_animations(b.v, 0, 0, table, in);
    CHECK(in.flags == nullptr && in.animation_count == 0);
}
