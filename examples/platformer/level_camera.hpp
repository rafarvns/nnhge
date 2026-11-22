#pragma once
// The level's camera: an nnh::CameraRig (spec 018) and its settings (pure, tested in
// tests/platformer/test_level_camera.cpp).

#include <nnh/camera.hpp>

/// The rig's settings for a level whose map covers `bounds`: the player's box centred, no dead zone, no smoothing,
/// limits from the map with the view's left/top edge at the map's on an axis where the map is smaller than the
/// view — exactly the hand-written camera this example had before spec 018 (its replay checksums didn't change).
nnh::CameraSettings level_camera_settings(nnh::Rect bounds);
