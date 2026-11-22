#pragma once
// The level's camera: an nnh::CameraRig (spec 018) and its settings, plus the look-ahead and landing-shake
// rules (pure, tested in tests/platformer/test_level_camera.cpp).
//
// History: spec 018 first configured the rig exactly like the hand-written camera this example had (player
// centred, no dead zone, no smoothing, map limits with Start alignment: the replay's checksums didn't change,
// AC-4 step 1), then gave it the feel below (step 2, new checksums in tests/level.nnhs).

#include <nnh/camera.hpp>

/// Pixels the view looks ahead of the player, in the direction it faces.
inline constexpr float kLookAhead = 32;
/// The look-ahead moves this many pixels per frame when the player turns (no jump of the camera).
inline constexpr float kLookAheadSpeed = 1;
/// Falling at least this fast (pixels per frame: the player's top speed) makes a landing shake the camera.
inline constexpr float kHardLandingSpeed = 6;
/// Trauma of a hard landing.
inline constexpr float kLandingTrauma = 0.5f;

/// The rig's settings for a level whose map covers `bounds`: a 32 × 48 dead zone, smoothing on y only (jumps
/// don't jerk the view; runs stay crisp), a small shake, limits from the map with the view's left/top edge at
/// the map's on an axis where the map is smaller than the view.
nnh::CameraSettings level_camera_settings(nnh::Rect bounds);

/// The next frame's look-ahead (the rig's offset x): `current` moved kLookAheadSpeed towards ±kLookAhead.
float look_ahead_step(float current, bool facing_left);

/// The trauma a landing adds after falling at `fall_speed` pixels per frame: kLandingTrauma from
/// kHardLandingSpeed, 0 below.
float landing_trauma(float fall_speed);
