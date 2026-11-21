// Box queries on values (spec 012 FR-9). Edges are half-open: a box covers [x, x + w) x [y, y + h).

#include "nnh/collision.hpp"

namespace nnh::collision {

namespace {

float min_of(float a, float b) { return a < b ? a : b; }
float max_of(float a, float b) { return a > b ? a : b; }

} // namespace

bool overlaps(Rect a, Rect b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && a.x < b.x + b.w && b.x < a.x + a.w &&
           a.y < b.y + b.h && b.y < a.y + a.h;
}

bool contains(Rect r, Vec2 p) { return r.x <= p.x && p.x < r.x + r.w && r.y <= p.y && p.y < r.y + r.h; }

Rect intersection(Rect a, Rect b) {
    if (!overlaps(a, b)) return {};
    const float x = max_of(a.x, b.x);
    const float y = max_of(a.y, b.y);
    return {x, y, min_of(a.x + a.w, b.x + b.w) - x, min_of(a.y + a.h, b.y + b.h) - y};
}

Vec2 separation(Rect a, Rect b) {
    const Rect shared = intersection(a, b);
    if (shared.w <= 0) return {};
    // Compare doubled centres (x * 2 + w) so no division is needed; ties push towards negative.
    const bool a_first_x = a.x * 2 + a.w <= b.x * 2 + b.w;
    const bool a_first_y = a.y * 2 + a.h <= b.y * 2 + b.h;
    // Push out on the side a sits on: depth measured from that side, so the boxes end flush.
    const float depth_x = a_first_x ? a.x + a.w - b.x : b.x + b.w - a.x;
    const float depth_y = a_first_y ? a.y + a.h - b.y : b.y + b.h - a.y;
    if (depth_x <= depth_y) return {a_first_x ? -depth_x : depth_x, 0};
    return {0, a_first_y ? -depth_y : depth_y};
}

} // namespace nnh::collision
