// Nintendo 3DS: 3D (contract nnh/platform/video3d.hpp). Stage 2 stub: 3D isn't built yet (stage 7 adds citro3d).

#include "nnh/platform/video3d.hpp"

namespace nnh::platform {

bool video3d_init(const Video3DConfig&) { return false; }
void video3d_shutdown() {}
bool mesh_create(const MeshUpload&, MeshInfo& out) {
    out = {};
    return false;
}
void mesh_destroy(MeshHandle) {}
void begin_3d(Screen, const Pass3D&) {}
void draw_3d(const Draw3D&) {}
void end_3d() {}

} // namespace nnh::platform
