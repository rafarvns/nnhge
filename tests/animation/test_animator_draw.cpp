// Drawing (T3.3; FR-10): region, pivot (offset applied, mirrored by the flips) and draw_options(); a trimmed frame,
// its untrimmed original and both flipped land on the same pixels (from the fake platform's recorded draw items).

#include "../gfx/gfx_fixture.hpp"
#include "library_fixture.hpp"

using nnh::Region;
using nnh::SpriteAnimator;
using nnh::Vec2;

namespace {

/// Frame 0: an untrimmed 16×16 cell whose opaque pixels are (3..12, 4..15). Frame 1: the same art trimmed to its
/// opaque 10×12 pixels, at (16, 0) in the texture, offset (3, 4) in the 16×16 frame.
nnh::SpriteLibrary trimmed_pair() {
    auto made = nnh::SpriteLibrary::create({.frames = 2, .clips = 2, .clip_frames = 2,
                                            .pivot = nnh::SpritePivot::BottomCenter});
    nnh::SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_frame({.region = {0, 0, 16, 16}}).ok());
    CHECK(lib.add_frame({.region = {16, 0, 10, 12}, .offset_x = 3, .offset_y = 4, .source_width = 16,
                         .source_height = 16})
              .ok());
    CHECK(lib.add_clip({.name = "full", .first = 0, .count = 1}).ok());
    CHECK(lib.add_clip({.name = "trim", .first = 1, .count = 1}).ok());
    return lib;
}

struct Rect {
    float x0, y0, x1, y1;
    bool operator==(const Rect&) const = default;
};

/// The world rectangle covered by the opaque source pixels `opaque` (in region coordinates) of a drawn item.
Rect opaque_rect(const nnh::platform::DrawItem2D& item, Region opaque) {
    const float left = item.x - item.pivot_x;
    const float top = item.y - item.pivot_y;
    const float x0 = item.flip_x ? item.w - static_cast<float>(opaque.x + opaque.w) : static_cast<float>(opaque.x);
    const float y0 = item.flip_y ? item.h - static_cast<float>(opaque.y + opaque.h) : static_cast<float>(opaque.y);
    return {left + x0, top + y0, left + x0 + static_cast<float>(opaque.w), top + y0 + static_cast<float>(opaque.h)};
}

} // namespace

NNH_TEST(the_animator_reports_the_frames_region_pivot_and_offset) {
    nnh::SpriteLibrary lib = trimmed_pair();
    SpriteAnimator a{lib};
    a.play(lib.find_clip("trim"));
    const Region r = a.region();
    CHECK(r.x == 16 && r.y == 0 && r.w == 10 && r.h == 12);
    CHECK(a.offset() == (Vec2{3, 4}));
    CHECK(a.pivot() == (Vec2{5, 12})); // bottom-centre (8, 16) of the frame, minus the offset
    a.set_flip(true, true);
    CHECK(a.pivot() == (Vec2{5, 0}));
    const nnh::DrawOptions o = a.draw_options();
    CHECK(o.region.x == 16 && o.region.w == 10 && o.pivot == (Vec2{5, 0}) && o.flip_x && o.flip_y);
}

NNH_TEST(trimmed_and_untrimmed_frames_land_on_the_same_pixels_flipped_or_not) {
    gfx_test::restart();
    nnh::Texture texture = gfx_test::load("hero");
    nnh::SpriteLibrary lib = trimmed_pair();
    SpriteAnimator full{lib};
    SpriteAnimator trim{lib};
    full.play(lib.find_clip("full"));
    trim.play(lib.find_clip("trim"));
    const bool flips[4][2] = {{false, false}, {true, false}, {false, true}, {true, true}};
    for (const auto& f : flips) {
        full.set_flip(f[0], f[1]);
        trim.set_flip(f[0], f[1]);
        CHECK(nnh::frame());
        nnh::gfx::draw(nnh::Screen::Top, texture, {100, 80}, full.draw_options());
        nnh::gfx::draw(nnh::Screen::Top, texture, {100, 80}, trim.draw_options());
        CHECK(nnh::frame());
        CHECK(nnh::platform_fake::state().last_frame_item_count == 2);
        const auto& a = nnh::platform_fake::state().last_frame_items[0];
        const auto& b = nnh::platform_fake::state().last_frame_items[1];
        CHECK(a.flip_x == f[0] && b.flip_y == f[1]);
        const Rect want = opaque_rect(a, {3, 4, 10, 12});
        CHECK(opaque_rect(b, {0, 0, 10, 12}) == want);
    }
    // The last draws flip both ways: the art (centred in its cell, 3 + 10 + 3) hangs from the pivot, mirrored.
    const Rect last = opaque_rect(nnh::platform_fake::state().last_frame_items[0], {3, 4, 10, 12});
    CHECK(last == (Rect{95, 80, 105, 92}));
    texture.reset();
    nnh::shutdown();
}

NNH_TEST(no_clip_draws_nothing_special) {
    nnh::SpriteLibrary lib = trimmed_pair();
    SpriteAnimator a{lib};
    const nnh::DrawOptions o = a.draw_options();
    CHECK(o.region.empty() && o.pivot == (Vec2{0, 0}));
    CHECK(a.pivot() == (Vec2{0, 0}) && a.offset() == (Vec2{0, 0}));
}
