// Mesh records (mesh_internal.hpp): one heap block per mesh, its data uploaded through the platform and accounted in
// the Linear region under `meshes` on every system (FR-8).

#include <new>

#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"

namespace nnh::detail {

Material3D material_from_record(const mesh::MaterialRecord& r) {
    Material3D m;
    m.base_color = {r.base[0], r.base[1], r.base[2], r.base[3]};
    m.specular = {r.specular[0], r.specular[1], r.specular[2], 255};
    m.shininess = r.shininess;
    m.alpha = static_cast<AlphaMode>(r.alpha);
    m.alpha_cutoff = r.cutoff;
    m.filter = static_cast<TextureFilter>(r.filter);
    m.wrap_u = static_cast<TextureWrap>(r.wrap_u);
    m.wrap_v = static_cast<TextureWrap>(r.wrap_v);
    m.lit = (r.flags & mesh::kMatLit) != 0;
    m.vertex_colors = (r.flags & mesh::kMatVertexColors) != 0;
    m.cull_back = (r.flags & mesh::kMatCullBack) != 0;
    return m;
}

MeshRecord* mesh_record_create(const mesh::MeshView& view, const char* what, Error& error) {
    const mesh::MeshHeader& h = view.header;
    const std::size_t bytes = sizeof(MeshRecord) + sizeof(SubmeshRuntime) * h.submesh_count +
                              sizeof(Texture) * h.texture_count;
    void* block = mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(meshes), bytes, alignof(MeshRecord));
    if (block == nullptr) {
        error = Error::make(ErrorCode::OutOfMemory, "%s: no memory for the mesh", what);
        return nullptr;
    }
    auto* r = new (block) MeshRecord;
    r->block_bytes = bytes;
    r->vertex_count = static_cast<int>(h.vertex_count);
    r->index_count = static_cast<int>(h.index_count);
    r->submesh_count = h.submesh_count;
    for (int a = 0; a < 3; ++a) r->pos_scale[a] = h.pos_scale[a], r->pos_offset[a] = h.pos_offset[a];
    r->bounds = {{h.aabb_min[0], h.aabb_min[1], h.aabb_min[2]}, {h.aabb_max[0], h.aabb_max[1], h.aabb_max[2]}};
    r->sphere = {{h.sphere[0], h.sphere[1], h.sphere[2]}, h.sphere[3]};
    r->submeshes = reinterpret_cast<SubmeshRuntime*>(r + 1);
    r->textures = reinterpret_cast<Texture*>(r->submeshes + h.submesh_count);
    for (int s = 0; s < h.submesh_count; ++s) {
        const mesh::SubmeshRecord rec = mesh::submesh(view, s);
        SubmeshRuntime* sm = new (r->submeshes + s) SubmeshRuntime;
        sm->first_index = rec.first_index;
        sm->index_count = rec.index_count;
        sm->uv_scale[0] = rec.uv_scale[0], sm->uv_scale[1] = rec.uv_scale[1];
        sm->uv_offset[0] = rec.uv_offset[0], sm->uv_offset[1] = rec.uv_offset[1];
        sm->material = material_from_record(rec.material);
    }
    for (int t = 0; t < h.texture_count; ++t) new (r->textures + t) Texture;
    r->texture_count = h.texture_count;
    platform::MeshUpload up;
    up.vertices = view.vertices;
    up.vertex_count = h.vertex_count;
    up.stride = h.stride;
    up.flags = h.flags;
    up.indices = reinterpret_cast<const std::uint16_t*>(view.indices);
    up.index_count = h.index_count;
    platform::MeshInfo info;
    if (!platform::mesh_create(up, info)) {
        NNH_LOG_WARN(gfx, "%s: %s", what, platform::last_error());
        error = Error::make(ErrorCode::OutOfMemory, "%s: the GPU copy failed", what);
        mesh_record_destroy(r);
        return nullptr;
    }
    r->handle = info.handle;
    r->linear_bytes = info.bytes;
    mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(meshes), info.bytes, true);
    return r;
}

void mesh_record_destroy(MeshRecord* r) {
    if (r == nullptr) return;
    if (r->handle != 0) {
        platform::mesh_destroy(r->handle);
        mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(meshes), r->linear_bytes, false);
    }
    for (int t = 0; t < r->texture_count; ++t) r->textures[t].~Texture();
    r->~MeshRecord();
    mem::free(r);
}

} // namespace nnh::detail
