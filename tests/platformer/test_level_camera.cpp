// The platformer's camera (examples/platformer/level_camera.hpp): an nnh::CameraRig configured like the hand-written
// follow_camera() the example had before spec 018 gives the same bits (AC-4 step 1).

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

} // namespace

NNH_TEST(level_camera_matches_the_old_follow_camera_bit_for_bit) {
    const nnh::Vec2 worlds[] = {{2048, 320}, {1600, 200}, {400, 240}, {300, 100}, {1024, 1024}};
    int mismatches = 0;
    for (nnh::Vec2 world : worlds) {
        nnh::CameraRig rig(level_camera_settings({0, 0, world.x, world.y}));
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

NNH_TEST(level_camera_centres_the_player_and_stops_at_the_map_edges) {
    nnh::CameraRig rig(level_camera_settings({0, 0, 2048, 320}));
    rig.snap(nnh::Rect{1000, 158, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{805, 45}));
    rig.update(nnh::Rect{2040, 300, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{1648, 80}));
    rig.update(nnh::Rect{20, 10, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{0, 0}));
}

NNH_TEST(level_camera_keeps_a_small_map_at_its_top_left) {
    nnh::CameraRig rig(level_camera_settings({0, 0, 1600, 200}));
    rig.update(nnh::Rect{600, 100, 10, 14});
    CHECK(rig.position() == (nnh::Vec2{405, 0}));
}
