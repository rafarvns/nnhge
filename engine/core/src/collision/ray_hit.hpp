#pragma once
// Filling a RayHit (spec 019 FR-8): shared by the ray and cast files. Internal, pure.

#include "collision/exact.hpp"

namespace nnh::collision::detail {

/// |to − from| in float: the only square root of the queries (ray_distance.cpp, -fno-math-errno: one instruction).
float ray_length(Vec2 from, Vec2 to);

/// True as soon as one box passing `filter` is crossed by from → to, starts inside excluded (line of sight;
/// raycast_boxes.cpp). The view and points must have been checked.
bool any_box_on_ray(const BoxView& boxes, Vec2 from, Vec2 to, const QueryFilter& filter);

/// A hit of something containing the start: fraction 0, point `from`, normal (0, 0).
inline RayHit inside_hit(Vec2 from) {
    RayHit h;
    h.point = from;
    h.inside = true;
    return h;
}

/// The normal of the face entered on x (`face_x`) or y, against the segment's direction.
inline Vec2 face_normal(const Segment& s, bool face_x) {
    return face_x ? Vec2{static_cast<float>(-s.sx), 0} : Vec2{0, static_cast<float>(-s.sy)};
}

} // namespace nnh::collision::detail
