// Limits (spec 018 FR-6): the view stays inside a world rectangle; alignment where the world is smaller; clamping
// before smoothing (no easing into a wall) and after the shake; limits changed at run time (glide or cut).

#include "camera/camera_internal.hpp"
#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraAlign;
using nnh::CameraRig;
using nnh::Rect;
using nnh::Vec2;
namespace cam = nnh::detail::cam;

namespace {

constexpr Rect kWorld{0, 0, 2048, 320};

} // namespace

// Part 1: no smoothing.

NNH_TEST(limits_stop_the_view_at_the_left_and_top_edges) {
    CameraRig rig;
    rig.set_limits(kWorld);
    rig.update(Rect{20, 10, 10, 14});
    CHECK(at(rig.position(), 0, 0));
    CHECK(rig.settings().has_limits);
}

NNH_TEST(limits_stop_the_view_at_the_right_and_bottom_edges) {
    CameraRig rig;
    rig.set_limits(kWorld);
    rig.update(Rect{2040, 300, 10, 14});
    CHECK(at(rig.position(), 1648, 80));
}

NNH_TEST(limits_with_an_offset_origin_are_respected) {
    CameraRig rig;
    rig.set_limits({-100, 500, 600, 400});
    rig.update(Vec2{-1000, 0});
    CHECK(at(rig.position(), -100, 500));
    rig.update(Vec2{1000, 2000});
    CHECK(at(rig.position(), 100, 660));
}

NNH_TEST(a_small_world_is_centred_by_default) {
    CameraRig rig;
    rig.set_limits({0, 0, 1600, 200}); // 200 tall, view 240
    rig.update(Vec2{600, 100});
    CHECK(at(rig.position(), 400, -20));
}

NNH_TEST(a_small_world_can_align_to_its_start_or_end) {
    CameraRig start({.small_world = CameraAlign::Start});
    start.set_limits({0, 0, 1600, 200});
    start.update(Vec2{600, 100});
    CHECK(at(start.position(), 400, 0));
    CameraRig end({.small_world = CameraAlign::End});
    end.set_limits({0, 0, 1600, 200});
    end.update(Vec2{600, 100});
    CHECK(at(end.position(), 400, -40));
}

NNH_TEST(clear_limits_lets_the_view_go_anywhere) {
    CameraRig rig;
    rig.set_limits(kWorld);
    rig.clear_limits();
    rig.update(Vec2{0, 0});
    CHECK(at(rig.position(), -200, -120));
    CHECK(!rig.settings().has_limits);
}

NNH_TEST(clamp_axis_is_exactly_at_the_edge_when_the_target_is_centred_on_it) {
    CHECK(cam::clamp_axis(0, 0, 2048, 400, CameraAlign::Center) == 0);
    CHECK(cam::clamp_axis(1648, 0, 2048, 400, CameraAlign::Center) == 1648);
    CHECK(cam::clamp_axis(1648.5f, 0, 2048, 400, CameraAlign::Center) == 1648);
    CHECK(cam::clamp_axis(-0.5f, 0, 2048, 400, CameraAlign::Center) == 0);
    CHECK(cam::clamp_axis(5, 0, 400, 400, CameraAlign::Center) == 0); // exactly the view: hi == lo
}

NNH_TEST(final_clamp_keeps_the_followed_range_and_skips_small_axes) {
    CHECK(cam::final_clamp_axis(-3, 0, 0, 2048, 400) == 0);
    CHECK(cam::final_clamp_axis(1650, 1640, 0, 2048, 400) == 1648);
    CHECK(cam::final_clamp_axis(1700, 1690, 0, 2048, 400) == 1690); // gliding in from outside: kept at followed
    CHECK(cam::final_clamp_axis(-30, -20, 0, 200, 240) == -30);     // small axis: unclamped
}

// Part 2: smoothing.

NNH_TEST(the_desired_position_is_clamped_before_smoothing_so_the_camera_never_eases_into_a_wall) {
    CameraRig rig({.smoothing = {0.5f, 0.5f}});
    rig.set_limits(kWorld);
    rig.snap(Vec2{1500, 160});
    float last = rig.position().x;
    for (int i = 0; i < 60; ++i) {
        rig.update(Vec2{3000, 160}); // far past the right edge
        CHECK(rig.position().x >= last && rig.position().x <= 1648);
        last = rig.position().x;
    }
    CHECK(rig.position().x == 1648); // settled exactly on the edge, never past it
}

NNH_TEST(new_limits_with_smoothing_glide_the_camera_in) {
    CameraRig rig({.smoothing = {0.25f, 1}});
    rig.set_limits({0, 0, 1000, 240});
    rig.snap(Vec2{900, 120}); // at the right edge: 600
    CHECK(rig.position().x == 600);
    rig.set_limits({1000, 0, 1000, 240}); // the next room
    rig.update(Vec2{1100, 120});
    CHECK(rig.position().x == 700); // a quarter of the way to 1000, not a jump
    cam_test::run(rig, Vec2{1100, 120}, 200);
    CHECK(rig.position().x == 1000);
}

NNH_TEST(new_limits_with_snap_cut_to_the_new_room) {
    CameraRig rig({.smoothing = {0.25f, 1}});
    rig.set_limits({0, 0, 1000, 240});
    rig.snap(Vec2{900, 120});
    rig.set_limits({1000, 0, 1000, 240});
    rig.snap(Vec2{1100, 120});
    CHECK(rig.position().x == 1000);
}
