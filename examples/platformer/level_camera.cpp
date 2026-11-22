#include "level_camera.hpp"

nnh::CameraSettings level_camera_settings(nnh::Rect bounds) {
    return {.small_world = nnh::CameraAlign::Start, .has_limits = true, .limits = bounds};
}
