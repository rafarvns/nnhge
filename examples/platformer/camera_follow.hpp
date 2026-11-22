#pragma once
// The camera that follows the player (pure, tested in tests/platformer/test_camera_follow.cpp).

#include <nnh/math.hpp>

/// The camera's top-left corner that centres `target` in a `view`-sized window, clamped so the view stays
/// inside a world of `world` pixels starting at (0, 0). On an axis where the world is smaller than the view
/// the camera stays at 0.
nnh::Vec2 follow_camera(nnh::Rect target, nnh::Vec2 view, nnh::Vec2 world);
