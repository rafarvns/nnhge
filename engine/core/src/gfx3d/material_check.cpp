// Material validation (gfx3d_state.hpp, FR-9/FR-29): enums and shininess in range; an invalid value is clamped.

#include "gfx3d/gfx3d_state.hpp"

namespace nnh::detail {

bool check_material(Material3D& m) {
    bool ok = true;
    if (m.shininess > kMaxShininess) m.shininess = kMaxShininess, ok = false;
    if (static_cast<int>(m.alpha) > 2) m.alpha = AlphaMode::Opaque, ok = false;
    if (static_cast<int>(m.filter) > 1) m.filter = TextureFilter::Nearest, ok = false;
    if (static_cast<int>(m.wrap_u) > 2) m.wrap_u = TextureWrap::Repeat, ok = false;
    if (static_cast<int>(m.wrap_v) > 2) m.wrap_v = TextureWrap::Repeat, ok = false;
    return ok;
}

} // namespace nnh::detail
