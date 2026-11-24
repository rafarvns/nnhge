// The path follower (spec 025 FR-19): squared distances against a squared radius, + - x and compares only (no square
// root), built with -ffp-contract=off: the same bits everywhere. Never moves anything.

#include "pathfinding/path_check.hpp"

namespace nnh {
namespace {

float distance2(Vec2 a, Vec2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool finite(float v) { return v == v && v - v == 0; }

} // namespace

PathFollower::PathFollower(const Vec2* points, int count, float arrival_radius) {
    reset(points, count, arrival_radius);
}

void PathFollower::reset(const Vec2* points, int count, float arrival_radius) {
    points_ = nullptr;
    count_ = 0;
    next_ = 0;
    radius2_ = 0;
    arrived_ = true;
    if (!detail::path_buffer_ok(points, count, "PathFollower::reset")) return;
    if (!finite(arrival_radius) || arrival_radius < 0) {
        detail::path_misuse("PathFollower::reset: the arrival radius must be finite and >= 0");
        return;
    }
    points_ = points;
    count_ = count;
    radius2_ = arrival_radius * arrival_radius;
    arrived_ = count == 0;
}

Vec2 PathFollower::target(Vec2 position) {
    if (!detail::path_point_ok(position, "PathFollower::target")) return count_ > 0 ? points_[next_] : position;
    if (count_ == 0) {
        arrived_ = true;
        return position;
    }
    while (next_ < count_ - 1 && distance2(position, points_[next_]) <= radius2_) ++next_;
    arrived_ = next_ == count_ - 1 && distance2(position, points_[next_]) <= radius2_;
    return points_[next_];
}

bool PathFollower::arrived() const { return arrived_; }
int PathFollower::index() const { return next_; }

} // namespace nnh
