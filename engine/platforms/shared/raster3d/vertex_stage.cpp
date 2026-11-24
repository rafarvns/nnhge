// The vertex stage (raster3d.hpp): what the console's vertex shader does — raw int16 positions through the model-view
// (the quantisation decode folded in), int8 normals through the normal matrix, uv decoded, colours / 255 — then the
// projection. Fixed operation order.

#include <cstring>

#include "raster3d.hpp"

namespace nnh::raster3d {

ClipVertex vertex_stage(const platform::Pass3D& pass, const platform::Draw3D& d, const MeshView& mesh,
                        std::uint32_t index) {
    const std::uint8_t* src = mesh.vertices + static_cast<std::size_t>(index) * mesh.stride;
    std::int16_t q[4];
    std::int8_t n[4];
    std::memcpy(q, src, 8);
    std::memcpy(n, src + 8, 4);
    const float p[3] = {static_cast<float>(q[0]), static_cast<float>(q[1]), static_cast<float>(q[2])};
    ClipVertex v;
    const float* m = d.model_view;
    for (int r = 0; r < 3; ++r) v.attr[r] = ((m[r * 4] * p[0] + m[r * 4 + 1] * p[1]) + m[r * 4 + 2] * p[2]) + m[r * 4 + 3];
    const float nn[3] = {n[0] / 127.0f, n[1] / 127.0f, n[2] / 127.0f};
    for (int r = 0; r < 3; ++r)
        v.attr[3 + r] = (d.normal[r * 3] * nn[0] + d.normal[r * 3 + 1] * nn[1]) + d.normal[r * 3 + 2] * nn[2];
    int at = 12;
    if ((mesh.flags & 1) != 0) {
        std::int16_t uv[2];
        std::memcpy(uv, src + at, 4);
        v.attr[6] = static_cast<float>(uv[0]) * d.uv_scale[0] + d.uv_offset[0];
        v.attr[7] = static_cast<float>(uv[1]) * d.uv_scale[1] + d.uv_offset[1];
        at += 4;
    } else {
        v.attr[6] = v.attr[7] = 0.0f;
    }
    if ((mesh.flags & 2) != 0) {
        for (int c = 0; c < 4; ++c) v.attr[8 + c] = static_cast<float>(src[at + c]) / 255.0f;
    } else {
        v.attr[8] = v.attr[9] = v.attr[10] = v.attr[11] = 1.0f;
    }
    const float* P = pass.projection; // column-major
    for (int r = 0; r < 4; ++r)
        v.clip[r] = ((P[r] * v.attr[0] + P[4 + r] * v.attr[1]) + P[8 + r] * v.attr[2]) + P[12 + r];
    return v;
}

} // namespace nnh::raster3d
