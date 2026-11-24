// 3D stats and the overlay line (nnh/gfx3d.hpp stats, FR-28).

#include <cstdio>

#include "gfx3d/gfx3d_state.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh {

Stats3D gfx3d::stats() {
    detail::gfx3d_link();
    Stats3D s = detail::state3d().last;
    s.meshes = detail::mesh_count();
    s.mesh_bytes = detail::mesh_bytes();
    return s;
}

bool detail::gfx3d_overlay_line(char* out, int size) {
    const Stats3D& s = state3d().last;
    if (!state3d().ready || size <= 0) return false;
    std::snprintf(out, static_cast<std::size_t>(size), "3d %d/%d draws %d culled %d tris %d meshes %uKB", s.draws,
                  s.max_draws, s.culled, s.triangles[0] + s.triangles[1], s.meshes,
                  static_cast<unsigned>(s.mesh_bytes / 1024));
    return true;
}

} // namespace nnh
