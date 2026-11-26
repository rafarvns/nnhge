#include "tuning.hpp"
#include "level_camera.hpp"

nnh::CameraSettings level_camera_settings(nnh::Rect bounds) {
    return {.small_world = nnh::CameraAlign::Start,
            .has_limits = true,
            .dead_zone = nnh::CameraSettings::centered_zone(32, 48),
            .smoothing = {1, 0.15f},
            .limits = bounds,
            .max_shake = {4, 6}};
}

float look_ahead_step(float current, bool facing_left) {
    const float ahead = g_tuning.look_ahead, speed = g_tuning.look_ahead_speed; // live values (spec 030)
    const float goal = facing_left ? -ahead : ahead;
    if (current < goal) return current + speed > goal ? goal : current + speed;
    if (current > goal) return current - speed < goal ? goal : current - speed;
    return current;
}

float landing_trauma(float fall_speed) {
    return fall_speed >= kHardLandingSpeed ? kLandingTrauma : 0.0f;
}
