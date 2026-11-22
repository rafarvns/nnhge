// Frames of a sheet (T1.3; FR-1, FR-2, FR-11): grids, explicit regions, trim, pivots, errors naming the frame, and
// frames used alone (no clips).

#include <cstring>

#include "library_fixture.hpp"
#include "nnh/gfx.hpp"

using nnh::Region;
using nnh::SpriteLibrary;
using nnh::Vec2;

namespace {

SpriteLibrary make(int frames, nnh::SpritePivot pivot = nnh::SpritePivot::TopLeft) {
    return std::move(SpriteLibrary::create({.frames = frames, .pivot = pivot}).value());
}

bool region_is(const nnh::SpriteFrame& f, Region r) {
    return f.region.x == r.x && f.region.y == r.y && f.region.w == r.w && f.region.h == r.h;
}

} // namespace

NNH_TEST(a_grid_is_read_row_by_row) {
    SpriteLibrary lib = make(8);
    auto first = lib.add_grid({.frame_width = 16, .frame_height = 8, .count = 5, .columns = 3});
    CHECK(first.ok() && first.value() == 0 && lib.frame_count() == 5);
    CHECK(region_is(lib.frame(0), {0, 0, 16, 8}));
    CHECK(region_is(lib.frame(2), {32, 0, 16, 8}));
    CHECK(region_is(lib.frame(3), {0, 8, 16, 8}));
    CHECK(region_is(lib.frame(4), {16, 8, 16, 8}));
}

NNH_TEST(a_grid_with_margin_and_spacing_from_the_image_width) {
    SpriteLibrary lib = make(8);
    // 2 + 16 + 1 + 16 + 1 + 16 + 2 = 54 ≤ 56: three columns.
    CHECK(lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 4, .image_width = 56, .image_height = 64,
                        .margin = 2, .spacing = 1})
              .ok());
    CHECK(region_is(lib.frame(1), {19, 2, 16, 16}));
    CHECK(region_is(lib.frame(2), {36, 2, 16, 16}));
    CHECK(region_is(lib.frame(3), {2, 19, 16, 16}));
}

NNH_TEST(a_second_grid_continues_the_frame_indices) {
    SpriteLibrary lib = make(8);
    CHECK(lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 2, .columns = 2}).value() == 0);
    auto second = lib.add_grid({.frame_width = 4, .frame_height = 4, .count = 3, .columns = 3});
    CHECK(second.ok() && second.value() == 2 && lib.frame_count() == 5);
}

NNH_TEST(grid_errors_are_returned_and_add_nothing) {
    SpriteLibrary lib = make(4);
    CHECK(!lib.add_grid({.frame_width = 0, .frame_height = 8, .count = 1, .columns = 1}).ok());
    CHECK(!lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 0, .columns = 1}).ok());
    CHECK(!lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 1}).ok()); // no columns, no image width
    CHECK(!lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 1, .columns = 1, .margin = -1}).ok());
    auto too_tall = lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 4, .columns = 2, .image_width = 16,
                                  .image_height = 8});
    CHECK(!too_tall.ok() && std::strstr(too_tall.error().message, "frame 2") != nullptr);
    auto too_wide = lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 1, .columns = 3, .image_width = 16});
    CHECK(!too_wide.ok());
    auto full = lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 5, .columns = 5});
    CHECK(!full.ok() && full.error().code == nnh::ErrorCode::LimitReached);
    CHECK(std::strstr(full.error().message, "frame 4") != nullptr);
    CHECK(lib.frame_count() == 0);
}

NNH_TEST(explicit_regions_and_their_errors_name_the_frame) {
    SpriteLibrary lib = make(3);
    CHECK(lib.add_frame({.region = {5, 6, 7, 8}}).value() == 0);
    CHECK(region_is(lib.frame(0), {5, 6, 7, 8}));
    auto empty = lib.add_frame({.region = {0, 0, 0, 8}});
    CHECK(!empty.ok() && std::strstr(empty.error().message, "frame 1") != nullptr);
    CHECK(!lib.add_frame({.region = {-1, 0, 8, 8}}).ok());
    CHECK(!lib.add_frame({.region = {0, 0, 8, -2}}).ok());
    CHECK(!lib.add_frame({.region = {32760, 0, 16, 8}}).ok());
    CHECK(lib.add_frame({.region = {0, 0, 1, 1}}).ok());
    CHECK(lib.add_frame({.region = {0, 0, 1, 1}}).ok());
    auto full = lib.add_frame({.region = {0, 0, 1, 1}});
    CHECK(!full.ok() && full.error().code == nnh::ErrorCode::LimitReached);
}

NNH_TEST(trimmed_frames_keep_their_offset_and_check_their_source_size) {
    SpriteLibrary lib = make(4, nnh::SpritePivot::BottomCenter);
    CHECK(lib.add_frame({.region = {0, 0, 10, 12}, .offset_x = 3, .offset_y = 4, .source_width = 16,
                         .source_height = 16})
              .ok());
    const nnh::SpriteFrame f = lib.frame(0);
    CHECK(f.offset == (Vec2{3, 4}));
    CHECK(f.pivot == (Vec2{8, 16})); // bottom-centre of the untrimmed 16×16 frame
    CHECK(!lib.add_frame({.region = {0, 0, 10, 12}, .offset_x = 7, .source_width = 16, .source_height = 16}).ok());
    CHECK(!lib.add_frame({.region = {0, 0, 10, 12}, .offset_x = -1, .source_width = 16, .source_height = 16}).ok());
}

NNH_TEST(default_pivots_follow_the_library) {
    SpriteLibrary top = make(1);
    SpriteLibrary centre = make(1, nnh::SpritePivot::Center);
    SpriteLibrary bottom = make(1, nnh::SpritePivot::BottomCenter);
    CHECK(top.add_frame({.region = {0, 0, 15, 9}}).ok());
    CHECK(centre.add_frame({.region = {0, 0, 15, 9}}).ok());
    CHECK(bottom.add_frame({.region = {0, 0, 15, 9}}).ok());
    CHECK(top.frame(0).pivot == (Vec2{0, 0}));
    CHECK(centre.frame(0).pivot == (Vec2{7.5f, 4.5f})); // odd sizes: exact halves (1/16 px)
    CHECK(bottom.frame(0).pivot == (Vec2{7.5f, 9}));
}

NNH_TEST(explicit_pivots_are_kept_to_a_sixteenth_of_a_pixel) {
    SpriteLibrary lib = make(2, nnh::SpritePivot::BottomCenter);
    CHECK(lib.add_frame({.region = {0, 0, 16, 16}, .pivot = {3.25f, 14.5f}}).ok());
    CHECK(lib.frame(0).pivot == (Vec2{3.25f, 14.5f}));
    CHECK(!lib.add_frame({.region = {0, 0, 16, 16}, .pivot = {3000, 0}}).ok());
}

NNH_TEST(frames_work_without_clips) {
    SpriteLibrary lib = make(4, nnh::SpritePivot::Center);
    CHECK(lib.add_grid({.frame_width = 16, .frame_height = 16, .count = 4, .columns = 4}).ok());
    CHECK(lib.clip_count() == 0);
    const nnh::DrawOptions o = lib.draw_options(2);
    CHECK(o.region.x == 32 && o.region.w == 16 && o.pivot == (Vec2{8, 8}) && !o.flip_x);
    const nnh::DrawOptions flipped = lib.draw_options(2, true, true);
    CHECK(flipped.flip_x && flipped.flip_y && flipped.pivot == (Vec2{8, 8}));
    CHECK(lib.draw_options(9).region.empty());
    CHECK(lib.frame(-1).region.empty());
}
