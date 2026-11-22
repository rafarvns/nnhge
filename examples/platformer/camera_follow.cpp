#include "camera_follow.hpp"

namespace {

/// The view's start on one axis: `centre` in the middle, kept within [0, world - view].
float follow_axis(float centre, float view, float world) {
    float start = centre - view / 2;
    if (start > world - view) start = world - view;
    if (start < 0) start = 0;
    return start;
}

} // namespace

nnh::Vec2 follow_camera(nnh::Rect target, nnh::Vec2 view, nnh::Vec2 world) {
    return {follow_axis(target.x + target.w / 2, view.x, world.x),
            follow_axis(target.y + target.h / 2, view.y, world.y)};
}
