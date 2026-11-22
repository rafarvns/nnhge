// The platformer's camera (examples/platformer/level_camera.hpp): an nnh::CameraRig configured like the hand-written
// follow_camera() the example had before spec 018 gives the same bits (AC-4 step 1); the level's settings, look-ahead
// and landing shake (step 2).

#include "platformer/level_camera.hpp"
#include "test.hpp"

#include <bit>
#include <cstdint>

namespace {

// ---- A frozen copy of the old examples/platformer/camera_follow.cpp (removed by spec 018 T3.5) -------------------

float follow_axis(float centre, float view, float world) {
    float start = centre - view / 2;
    if (start > world - view) start = world - view;
    if (start < 0) start = 0;
    return start;
}

nnh::Vec2 follow_camera(nnh::Rect target, nnh::Vec2 view, nnh::Vec2 world) {
    return {follow_axis(target.x + target.w / 2, view.x, world.x),
            follow_axis(target.y + target.h / 2, view.y, world.y)};
}

// -------------------------------------------------------------------------------------------------------------------

bool same_bits(nnh::Vec2 a, nnh::Vec2 b) {
    return std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
           std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y);
}

constexpr nnh::Vec2 kView{400, 240};

/// Step 1's configuration (the old camera): no dead zone, no smoothing, map limits, Start alignment.
nnh::CameraSettings step1(nnh::Vec2 world) {
    return {.small_world = nnh::CameraAlign::Start, .has_limits = true, .limits = {0, 0, world.x, world.y}};
}

} // namespace

NNH_TEST(a_rig_configured_like_the_old_camera_matches_it_bit_for_bit) {
    const nnh::Vec2 worlds[] = {{2048, 320}, {1600, 200}, {400, 240}, {300, 100}, {1024, 1024}};
    int mismatches = 0;
    for (nnh::Vec2 world : worlds) {
        nnh::CameraRig rig(step1(world));
        rig.snap(nnh::Rect{0, 0, 10, 14});
        for (float x = -40.0f; x < world.x + 40; x += 3.37f) {
            for (float y = -40.0f; y < world.y + 40; y += 5.13f) {
                const nnh::Rect box{x, y, 10, 14};
                const nnh::Vec2 old = follow_camera(box, kView, world);
                rig.update(box);
                if (!same_bits(rig.position(), old) || !same_bits(rig.followed(), old)) ++mismatches;
                nnh::CameraRig snapped = rig;
                snapped.snap(box);
                if (!same_bits(snapped.position(), old)) ++mismatches;
            }
        }
    }
    CHECK(mismatches == 0);
}

NNH_TEST(level_camera_has_a_dead_zone_y_smoothing_a_small_shake_and_the_map_limits) {
    const nnh::CameraSettings s = level_camera_settings({0, 0, 2048, 320});
    CHECK(s.dead_zone.w == 32 && s.dead_zone.h == 48 && s.dead_zone.x == -16 && s.dead_zone.y == -24);
    CHECK(s.smoothing.x == 1 && s.smoothing.y > 0 && s.smoothing.y < 1);
    CHECK(s.has_limits && s.limits.w == 2048 && s.limits.h == 320 && s.small_world == nnh::CameraAlign::Start);
    CHECK(s.max_shake.x > 0 && s.max_shake.x <= 8 && s.max_shake.y <= 8);
}

NNH_TEST(level_camera_stays_inside_the_map_and_settles_on_a_still_player) {
    nnh::CameraRig rig(level_camera_settings({0, 0, 2048, 320}));
    rig.snap(nnh::Rect{1000, 158, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{805, 45}));
    for (int i = 0; i < 300; ++i) rig.update(nnh::Rect{2040, 300, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{1648, 80}));
    for (int i = 0; i < 300; ++i) rig.update(nnh::Rect{20, 10, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{0, 0}));
}

NNH_TEST(look_ahead_moves_one_pixel_per_frame_to_the_facing_side_and_stops) {
    CHECK(look_ahead_step(0, false) == 1 && look_ahead_step(0, true) == -1);
    CHECK(look_ahead_step(kLookAhead, false) == kLookAhead && look_ahead_step(-kLookAhead, true) == -kLookAhead);
    CHECK(look_ahead_step(31.5f, false) == kLookAhead); // never past it
    float x = kLookAhead;
    int frames = 0;
    while (x != -kLookAhead && frames < 1000) x = look_ahead_step(x, true), ++frames;
    CHECK(frames == 64); // a full turn takes about a second
}

NNH_TEST(only_a_landing_at_full_fall_speed_shakes) {
    CHECK(landing_trauma(5.9f) == 0 && landing_trauma(0) == 0 && landing_trauma(-3) == 0);
    CHECK(landing_trauma(kHardLandingSpeed) == kLandingTrauma && landing_trauma(9) == kLandingTrauma);
}
