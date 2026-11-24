// MeshDesc validation and sizing (mesh_build.hpp): every refusal names its reason.

#include <cstdarg>
#include <cstdio>

#include "mesh/mesh_build.hpp"
#include "mesh/mesh_format.hpp"

namespace nnh::detail::mesh {
namespace {

constexpr float kMaxCoordinate = 1048576.0f; // 2²⁰ units

bool fail(BuildInfo& info, const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vsnprintf(info.error, sizeof info.error, format, args);
    va_end(args);
    info.ok = false;
    return false;
}

bool in_range(float f) { // finite and |f| ≤ 2²⁰ (NaN fails both compares)
    return f <= kMaxCoordinate && f >= -kMaxCoordinate;
}
bool in_range(Vec3 v) { return in_range(v.x) && in_range(v.y) && in_range(v.z); }

bool valid_material(const Material3D& m) {
    return m.shininess <= kMaxShininess && static_cast<int>(m.alpha) <= 2 && static_cast<int>(m.filter) <= 1 &&
           static_cast<int>(m.wrap_u) <= 2 && static_cast<int>(m.wrap_v) <= 2;
}

bool check_indices(const MeshDesc& d, BuildInfo& info) {
    if (d.indices == nullptr) {
        if (d.vertex_count % 3 != 0) return fail(info, "no indices: the vertex count %d isn't a multiple of 3", d.vertex_count);
        if (d.index_count != 0 && d.index_count != d.vertex_count)
            return fail(info, "no indices but an index count of %d", d.index_count);
        info.index_count = d.vertex_count;
        return true;
    }
    if (d.index_count < 3 || d.index_count > kMaxIndices || d.index_count % 3 != 0)
        return fail(info, "index count %d: 3 to %d, a multiple of 3", d.index_count, kMaxIndices);
    for (int i = 0; i < d.index_count; ++i)
        if (d.indices[i] >= d.vertex_count)
            return fail(info, "index %d is %d: only %d vertices", i, d.indices[i], d.vertex_count);
    info.index_count = d.index_count;
    return true;
}

bool check_vertices(const MeshDesc& d, BuildInfo& info) {
    for (int v = 0; v < d.vertex_count; ++v) {
        if (!in_range(d.positions[v])) return fail(info, "position %d is not finite or beyond 2^20", v);
        if (d.normals != nullptr && !in_range(d.normals[v])) return fail(info, "normal %d is not finite", v);
        if (d.uvs != nullptr && !(in_range(d.uvs[v].x) && in_range(d.uvs[v].y)))
            return fail(info, "texture coordinate %d is not finite or beyond 2^20", v);
    }
    return true;
}

bool check_submeshes(const MeshDesc& d, const BuildExtras& e, BuildInfo& info) {
    if (d.submeshes == nullptr) {
        if (d.submesh_count != 0) return fail(info, "a submesh count of %d without submeshes", d.submesh_count);
        info.submesh_count = 1;
        return true;
    }
    if (d.submesh_count < 1 || d.submesh_count > kMaxSubmeshes)
        return fail(info, "submesh count %d: 1 to %d", d.submesh_count, kMaxSubmeshes);
    for (int s = 0; s < d.submesh_count; ++s) {
        const SubmeshDesc& sm = d.submeshes[s];
        if (sm.first_index < 0 || sm.index_count < 3 || sm.first_index % 3 != 0 || sm.index_count % 3 != 0 ||
            sm.index_count > info.index_count - sm.first_index)
            return fail(info, "submesh %d: indices %d..+%d don't fit %d indices in whole triangles", s,
                        sm.first_index, sm.index_count, info.index_count);
        if (!valid_material(sm.material)) return fail(info, "submesh %d: invalid material", s);
        if (e.submesh_textures != nullptr && e.submesh_textures[s] != kNoTexture &&
            e.submesh_textures[s] >= e.texture_count)
            return fail(info, "submesh %d: texture %d of %d", s, e.submesh_textures[s], e.texture_count);
    }
    info.submesh_count = d.submesh_count;
    return true;
}

} // namespace

bool valid_texture_path(const char* path) {
    if (path == nullptr || path[0] == '\0' || path[0] == '/') return false;
    int n = 0;
    for (; path[n] != '\0'; ++n) {
        const char c = path[n];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                        c == '.' || c == '/' || c == '-';
        if (!ok || n >= kPathBytes - 1) return false;
        if (c == '.' && path[n + 1] == '.') return false;
    }
    return true;
}

BuildInfo measure(const MeshDesc& d, const BuildExtras& e) {
    BuildInfo info;
    if (d.positions == nullptr) return fail(info, "no positions"), info;
    if (d.vertex_count < 1 || d.vertex_count > kMaxMeshVertices)
        return fail(info, "vertex count %d: 1 to %d", d.vertex_count, kMaxMeshVertices), info;
    if (!check_indices(d, info) || !check_vertices(d, info)) return info;
    if (e.texture_count < 0 || e.texture_count > kMaxTextures)
        return fail(info, "%d textures: at most %d", e.texture_count, kMaxTextures), info;
    for (int t = 0; t < e.texture_count; ++t)
        if (!valid_texture_path(e.texture_paths[t])) return fail(info, "texture %d: an invalid path", t), info;
    if (!check_submeshes(d, e, info)) return info;
    info.flat = d.normals == nullptr;
    info.vertex_count = info.flat ? info.index_count : d.vertex_count;
    if (info.vertex_count > kMaxMeshVertices)
        return fail(info, "flat normals need %d vertices: at most %d (give normals or split the mesh)",
                    info.vertex_count, kMaxMeshVertices), info;
    const std::uint8_t flags = static_cast<std::uint8_t>((d.uvs != nullptr ? kFlagUv : 0) |
                                                         (d.colors != nullptr ? kFlagColor : 0));
    info.size = layout(info.submesh_count, e.texture_count, info.vertex_count, vertex_stride(flags), info.index_count)
                    .file_size;
    info.ok = true;
    return info;
}

} // namespace nnh::detail::mesh
