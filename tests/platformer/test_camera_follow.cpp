// The platformer's camera (examples/platformer/camera_follow.hpp): centred on the player, clamped to the map.

#include "platformer/camera_follow.hpp"
#include "test.hpp"

namespace {

constexpr nnh::Vec2 kView{400, 240};
constexpr nnh::Vec2 kWorld{2048, 320};

bool at(nnh::Vec2 v, float x, float y) {
    return v.x == x && v.y == y;
}

} // namespace

NNH_TEST(camera_centres_the_target_in_the_view) {
    // Target centre (1005, 165): the view's centre (200, 120) goes there.
    CHECK(at(follow_camera({1000, 158, 10, 14}, kView, kWorld), 805, 45));
}

NNH_TEST(camera_stops_at_the_left_and_top_edges) {
    CHECK(at(follow_camera({20, 10, 10, 14}, kView, kWorld), 0, 0));
}

NNH_TEST(camera_stops_at_the_right_and_bottom_edges) {
    // The view's right edge at the world's (2048 - 400), its bottom at the world's (320 - 240).
    CHECK(at(follow_camera({2040, 300, 10, 14}, kView, kWorld), 1648, 80));
}

NNH_TEST(camera_stays_at_zero_on_an_axis_where_the_world_is_smaller_than_the_view) {
    CHECK(at(follow_camera({600, 100, 10, 14}, kView, {1600, 200}), 405, 0));
}

NNH_TEST(camera_is_exactly_at_the_edge_when_the_target_is_centred_on_it) {
    // Centre at x 200: the camera is exactly 0, not a rounding below it.
    CHECK(at(follow_camera({195, 113, 10, 14}, kView, kWorld), 0, 0));
}
