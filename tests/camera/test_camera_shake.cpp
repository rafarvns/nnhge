// Trauma shake (spec 018 FR-8): add and clamp, linear decay to exactly 0, offset bounded by max_shake / zoom × trauma²,
// seeded, never moving the followed position, kept inside the limits unless turned off.

#include "camera_fixture.hpp"

using cam_test::same_bits;
using nnh::CameraRig;
using nnh::Vec2;

namespace {

float absf(float v) { return v < 0 ? -v : v; }

} // namespace

NNH_TEST(trauma_adds_up_and_is_clamped_to_one_and_zero) {
    CameraRig rig;
    rig.add_trauma(0.4f);
    rig.add_trauma(0.4f);
    CHECK(rig.trauma() == 0.4f + 0.4f);
    rig.add_trauma(5);
    CHECK(rig.trauma() == 1);
    rig.add_trauma(-0.25f);
    CHECK(rig.trauma() == 0.75f);
    rig.add_trauma(-3);
    CHECK(rig.trauma() == 0);
}

NNH_TEST(trauma_decays_linearly_and_reaches_exactly_zero) {
    CameraRig rig({.trauma_decay = 0.25f});
    rig.add_trauma(1);
    rig.update(Vec2{});
    CHECK(rig.trauma() == 0.75f);
    rig.update(Vec2{});
    CHECK(rig.trauma() == 0.5f);
    cam_test::run(rig, Vec2{}, 5);
    CHECK(rig.trauma() == 0);
    CameraRig slow; // 1/60 per frame: full trauma lasts a second
    slow.add_trauma(1);
    cam_test::run(slow, Vec2{}, 59);
    CHECK(slow.trauma() > 0);
    cam_test::run(slow, Vec2{}, 2);
    CHECK(slow.trauma() == 0);
}

NNH_TEST(the_shake_uses_the_decayed_trauma_so_zero_trauma_means_no_shake) {
    CameraRig rig;
    rig.update(Vec2{1000, 500});
    rig.add_trauma(0.01f); // less than one frame of decay
    rig.update(Vec2{1000, 500});
    CHECK(rig.trauma() == 0 && same_bits(rig.position(), rig.followed()));
}

NNH_TEST(the_shake_never_moves_the_followed_position) {
    CameraRig calm({.smoothing = {0.3f, 0.3f}});
    CameraRig shaken = calm;
    shaken.add_trauma(1);
    for (int i = 0; i < 120; ++i) {
        const Vec2 target{static_cast<float>(i * 7), static_cast<float>(i * 3)};
        calm.update(target);
        shaken.update(target);
        CHECK(same_bits(calm.followed(), shaken.followed()));
    }
    CHECK(shaken.trauma() == 0 && same_bits(shaken.position(), calm.position()));
}

NNH_TEST(the_offset_is_bounded_by_max_shake_over_zoom_times_trauma_squared) {
    const float zooms[] = {1, 2, 0.5f};
    for (float z : zooms) {
        CameraRig rig({.zoom = z, .max_shake = {10, 4}, .trauma_decay = 0.001f, .shake_speed = 0.37f});
        rig.add_trauma(0.8f);
        float max_x = 0, max_y = 0;
        for (int i = 0; i < 300; ++i) {
            rig.update(Vec2{100, 100});
            const float t2 = rig.trauma() * rig.trauma();
            const Vec2 d = rig.position() - rig.followed();
            CHECK(absf(d.x) <= 10 / z * t2 + 1e-4f && absf(d.y) <= 4 / z * t2 + 1e-4f);
            max_x = absf(d.x) > max_x ? absf(d.x) : max_x;
            max_y = absf(d.y) > max_y ? absf(d.y) : max_y;
        }
        CHECK(max_x > 10 / z * 0.3f && max_y > 4 / z * 0.3f); // it does shake, on both axes
    }
}

NNH_TEST(the_same_seed_shakes_the_same_and_another_seed_differently) {
    CameraRig a, b, c({.seed = 12345});
    a.add_trauma(1);
    b.add_trauma(1);
    c.add_trauma(1);
    int differ = 0;
    for (int i = 0; i < 50; ++i) {
        a.update(Vec2{50, 50});
        b.update(Vec2{50, 50});
        c.update(Vec2{50, 50});
        CHECK(same_bits(a.position(), b.position()));
        if (!(a.position() == c.position())) ++differ;
    }
    CHECK(differ > 45);
}

NNH_TEST(stop_shake_puts_the_camera_back_on_the_followed_position) {
    CameraRig rig;
    rig.add_trauma(1);
    rig.update(Vec2{300, 300});
    rig.stop_shake();
    CHECK(rig.trauma() == 0 && same_bits(rig.position(), rig.followed()));
}

NNH_TEST(the_shake_stays_inside_the_limits_by_default) {
    CameraRig rig({.max_shake = {20, 20}, .trauma_decay = 0.001f});
    rig.set_limits({0, 0, 2048, 320});
    rig.add_trauma(1);
    for (int i = 0; i < 200; ++i) {
        rig.update(Vec2{0, 0}); // at the top-left corner of the level
        CHECK(rig.position().x >= 0 && rig.position().y >= 0);
    }
}

NNH_TEST(with_the_clamp_off_the_shake_may_pass_the_limits_by_up_to_max_shake) {
    CameraRig rig({.shake_inside_limits = false, .max_shake = {20, 20}, .trauma_decay = 0.001f});
    rig.set_limits({0, 0, 2048, 320});
    rig.add_trauma(1);
    bool passed = false;
    for (int i = 0; i < 200; ++i) {
        rig.update(Vec2{0, 0});
        CHECK(rig.position().x >= -20 && rig.position().y >= -20);
        if (rig.position().x < 0) passed = true;
    }
    CHECK(passed);
}

NNH_TEST(the_shake_is_not_clamped_on_a_small_world_axis) {
    CameraRig rig({.max_shake = {0, 20}, .trauma_decay = 0.001f});
    rig.set_limits({0, 0, 2048, 200}); // 200 tall, view 240: centred at y −20
    rig.add_trauma(1);
    bool moved = false;
    for (int i = 0; i < 100; ++i) {
        rig.update(Vec2{1000, 100});
        if (rig.position().y != -20) moved = true;
    }
    CHECK(moved);
}
