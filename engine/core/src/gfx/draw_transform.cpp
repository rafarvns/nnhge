// Drawing with a world transform (spec 022 FR-13): the position and the drawing rotation and scale merged into the
// given options (a world scale multiplies the given scale; the world rotation adds the given one, subtracted under a
// mirrored world as a child's rotation is). Everything else — region, pivot, flips, tint, opacity, layer — is kept.
// Its own object: a game that never draws a transform links none of it.

#include "nnh/gfx.hpp"
#include "nnh/transform.hpp"

#include <bit>
#include <cstdint>

namespace nnh::gfx {

DrawOptions transformed(const WorldTransform2D& world, const DrawOptions& given) {
    DrawOptions out = given;
    const std::uint32_t mirror =
        (std::bit_cast<std::uint32_t>(world.scale.x) ^ std::bit_cast<std::uint32_t>(world.scale.y)) & 0x80000000u;
    out.rotation = world.rotation + std::bit_cast<float>(std::bit_cast<std::uint32_t>(given.rotation) ^ mirror);
    out.scale = {world.scale.x * given.scale.x, world.scale.y * given.scale.y};
    return out;
}

void draw(Screen screen, const Texture& texture, const WorldTransform2D& world, const DrawOptions& options) {
    draw(screen, texture, world.position(), transformed(world, options));
}

} // namespace nnh::gfx
