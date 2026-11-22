// Following a target (spec 018 FR-2): a point or a box's centre, plus an offset, per-axis locks.

#include "camera_fixture.hpp"

using cam_test::at;
using nnh::CameraRig;
using nnh::Rect;
using nnh::Vec2;

NNH_TEST(rig_centres_a_point_target_in_the_top_view) {
    CameraRig rig;
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.followed(), 800, 380)); // view 400 × 240: half (200, 120)
    CHECK(at(rig.position(), 800, 380));
}

NNH_TEST(rig_centres_on_the_bottom_screen_with_its_width) {
    CameraRig rig({.screen = nnh::Screen::Bottom});
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.position(), 840, 380)); // 320 × 240
}

NNH_TEST(rig_follows_a_box_by_its_centre) {
    CameraRig rig;
    rig.update(Rect{1000, 158, 10, 14}); // centre (1005, 165)
    CHECK(at(rig.position(), 805, 45));
}

NNH_TEST(offset_is_added_to_the_target_and_can_change_between_updates) {
    CameraRig rig({.offset = {30, -10}});
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.position(), 830, 370));
    rig.set_offset({-30, 0});
    rig.update(Vec2{1000, 500});
    CHECK(at(rig.position(), 770, 380));
    CHECK(at(rig.settings().offset, -30, 0));
}

NNH_TEST(an_axis_not_followed_keeps_its_position) {
    CameraRig rig({.follow_y = false});
    rig.snap(Vec2{1000, 500});
    rig.update(Vec2{1200, 50});
    CHECK(at(rig.position(), 1000, 380));
    CameraRig horizontal({.follow_x = false});
    horizontal.snap(Vec2{1000, 500});
    horizontal.update(Vec2{1200, 50});
    CHECK(at(horizontal.position(), 800, -70));
}

NNH_TEST(passing_the_last_standing_height_keeps_y_while_jumping) {
    // A platformer follows y only on the ground by passing the height where the player last stood.
    CameraRig rig;
    float standing_y = 300;
    rig.snap(Vec2{100, standing_y});
    const float jump[] = {290, 270, 260, 270, 290, 300};
    for (float y : jump) {
        const bool on_ground = y == 300;
        if (on_ground) standing_y = y;
        rig.update(Vec2{100, standing_y});
        CHECK(rig.position().y == 180);
    }
}
