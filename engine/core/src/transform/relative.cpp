// Keep-world reparenting math (spec 022 FR-10, plan 022 "Attach"): the local that puts a node at `world` under
// `parent`, from the stored world values only (no sine, no chain walk): the position through the parent's inverse,
// the rotation and scale from the drawing values (exact without shear). Three divisions.

#include "transform/sin_cos.hpp"

namespace nnh {

bool relative(const WorldTransform2D& parent, const WorldTransform2D& world, Transform2D& out) {
    Vec2 position;
    if (!to_local(parent.map, world.map.origin, position)) return false;
    const std::uint32_t mirror = (detail::float_bits(parent.scale.x) ^ detail::float_bits(parent.scale.y)) &
                                 detail::kSignBit;
    const float turn = world.rotation - parent.rotation;
    out.position = position;
    out.rotation = std::bit_cast<float>(detail::float_bits(turn) ^ mirror);
    out.scale = {world.scale.x / parent.scale.x, world.scale.y / parent.scale.y};
    return true;
}

} // namespace nnh
