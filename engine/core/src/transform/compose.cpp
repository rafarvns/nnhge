// The public composition (spec 022 FR-1, FR-2): the same inline code as the hierarchy's pass and world_now.

#include "transform/compose.hpp"

namespace nnh {

WorldTransform2D to_world(const Transform2D& root) {
    WorldTransform2D out;
    detail::root_world(root, detail::sin_cos_inline(root.rotation), out);
    return out;
}

WorldTransform2D compose(const WorldTransform2D& parent, const Transform2D& local) {
    WorldTransform2D out;
    if (detail::pure_translation(local)) {
        detail::translate_world(parent, local.position, out);
    } else {
        detail::full_world(parent, local, detail::sin_cos_inline(local.rotation), out);
    }
    return out;
}

} // namespace nnh
