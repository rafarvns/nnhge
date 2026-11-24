// Drawing with a world transform (spec 022 FR-13, FR-4, T4.6): gfx::transformed merges position, rotation and scale
// into the given options (an animator's draw_options() included) and keeps everything else; a mirrored world subtracts
// the given rotation and carries the flip in the scale; flips aren't inherited; the draw overload reaches the platform
// with the world's position; camera zoom applies as to any draw.

#include "gfx_fixture.hpp"
#include "nnh/transform.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::load;
using gfx_test::restart;
using nnh::DrawOptions;
using nnh::Screen;
using nnh::Vec2;
using nnh::WorldTransform2D;

NNH_TEST(transformed_merges_rotation_and_scale_and_keeps_the_rest) {
    const WorldTransform2D w = nnh::to_world({.position = {40, 30}, .rotation = 0.5f, .scale = {2, 2}});
    const DrawOptions given{.region = {16, 0, 16, 16}, .pivot = {8, 8}, .rotation = 0.25f, .scale = {0.5f, 3},
                            .flip_x = true, .tint = nnh::rgba(1, 2, 3, 4), .opacity = 0.5f, .layer = 7,
                            .ignore_camera = true};
    const DrawOptions out = nnh::gfx::transformed(w, given);
    CHECK(out.rotation == 0.75f && out.scale == (Vec2{1, 6}));
    CHECK(out.region.x == 16 && out.pivot == (Vec2{8, 8}) && out.flip_x && !out.flip_y && out.layer == 7);
    CHECK(out.tint.r == 1 && out.tint.a == 4 && out.opacity == 0.5f && out.ignore_camera);
}

NNH_TEST(transformed_under_a_mirror_subtracts_the_given_rotation) {
    const WorldTransform2D w = nnh::to_world({.position = {0, 0}, .rotation = 0.5f, .scale = {-1, 1}});
    const DrawOptions out = nnh::gfx::transformed(w, {.rotation = 0.25f, .flip_y = true});
    CHECK(out.rotation == 0.25f && out.scale == (Vec2{-1, 1}) && out.flip_y && !out.flip_x);
    const DrawOptions plain = nnh::gfx::transformed(w);
    CHECK(plain.rotation == 0.5f && plain.scale == (Vec2{-1, 1}));
}

NNH_TEST(draw_with_a_world_transform_reaches_the_platform) {
    restart();
    nnh::Texture hero = load("hero");
    CHECK(nnh::frame());
    const WorldTransform2D w = nnh::to_world({.position = {64, 48}, .rotation = 0.5f, .scale = {-2, 2}});
    nnh::gfx::draw(Screen::Top, hero, w, {.region = {0, 0, 16, 16}, .pivot = {8, 8}});
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 1);
    const auto& s = fake::state().last_frame_items[0];
    CHECK(s.x == 64 && s.y == 48 && s.w == 32 && s.h == 32 && s.rotation == 0.5f && s.flip_x && !s.flip_y);
    nnh::shutdown();
}

NNH_TEST(draw_with_a_world_transform_is_zoomed_like_any_draw) {
    restart();
    nnh::Texture hero = load("hero");
    CHECK(nnh::frame());
    nnh::gfx::set_camera(Screen::Top, {.position = {0, 0}, .zoom = 2});
    nnh::gfx::draw(Screen::Top, hero, nnh::to_world({.position = {10, 5}}), {.region = {0, 0, 16, 16}});
    nnh::gfx::draw(Screen::Top, hero, Vec2{10, 5}, {.region = {0, 0, 16, 16}});
    CHECK(nnh::frame());
    const auto& a = fake::state().last_frame_items[0];
    const auto& b = fake::state().last_frame_items[1];
    CHECK(a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.x == 20);
    nnh::shutdown();
}
