// The platformer's player sprite (spec 016 T8.5, AC-8): the state its animator plays for each physics case, the
// footstep marker on the run frames, the flip from the facing, and the same pixels as the hand-written frame choice
// it replaced (frame 1 + (walk / 8) % 2 while running, 3 in the air, 0 standing).

#include <nnh/gfx.hpp>
#include <string_view>

#include "platformer/player_sprite.hpp"
#include "test.hpp"

using nnh::Vec2;

namespace {

PlayerLook look(bool ground, float vx, float vy, bool left = false) {
    return {.on_ground = ground, .velocity = {vx, vy}, .facing_left = left};
}

} // namespace

NNH_TEST(the_player_sprite_picks_its_state_from_the_physics) {
    PlayerSprite s;
    s.update(look(true, 0, 0));
    CHECK(s.state_name() == std::string_view("idle") && s.frame() == 0);
    s.update(look(true, 1.75f, 0));
    CHECK(s.state_name() == std::string_view("run") && s.frame() == 1);
    s.update(look(false, 0, -5));
    CHECK(s.state_name() == std::string_view("jump") && s.frame() == 3);
    s.update(look(false, 0, 2));
    CHECK(s.state_name() == std::string_view("fall") && s.frame() == 3);
}

NNH_TEST(running_shows_the_same_frames_as_the_old_counter) {
    PlayerSprite s;
    s.update(look(true, 0, 0));
    for (int walk = 1; walk <= 40; ++walk) {
        s.update(look(true, -1.75f, 0, true));
        CHECK(s.frame() == 1 + (walk / 8) % 2);
    }
    CHECK(s.draw_options().flip_x);
}

NNH_TEST(each_run_frame_entered_is_a_footstep) {
    PlayerSprite s;
    int steps = 0;
    for (int i = 0; i < 33; ++i) {
        s.update(look(true, 1.75f, 0));
        steps += s.stepped() ? 1 : 0;
    }
    CHECK(steps == 5); // frames 1 2 1 2 1 entered at updates 1, 9, 17, 25, 33
    CHECK(s.dust() > 0);
    s.update(look(true, 0, 0));
    CHECK(!s.stepped());
}

NNH_TEST(the_sprite_stands_on_the_box_like_before) {
    PlayerSprite s;
    s.update(look(true, 0, 0));
    const nnh::DrawOptions o = s.draw_options();
    CHECK(o.region.x == 0 && o.region.w == 16 && o.pivot == (Vec2{8, 16})); // bottom-centre: the box's feet
}
