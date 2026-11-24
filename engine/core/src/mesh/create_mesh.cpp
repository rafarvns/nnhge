// Meshes from the game's arrays (nnh/mesh.hpp create_mesh): the converter's builder writes the bytes (AC-4: the same
// as nnh-assetc), they are validated like a file, uploaded, and the game's materials (textures included) kept.

#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "mesh/mesh_build.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/log.hpp"

namespace nnh {

namespace detail {

/// Adds a built record to the table, or destroys it when the table is full.
Result<Mesh> mesh_publish(MeshRecord* record, const char* what) {
    std::uint32_t generation = 0;
    const int slot = mesh_add(record, generation);
    if (slot < 0) {
        mesh_record_destroy(record);
        NNH_LOG_WARN(gfx, "%s: more than %d meshes", what, kMeshSlots);
        return Error::make(ErrorCode::LimitReached, "%s: more than %d meshes", what, kMeshSlots);
    }
    return MeshAccess::make(slot, generation);
}

Error mesh_3d_off(const char* what) {
    NNH_LOG_WARN(gfx, "%s: 3D is off (AppConfig::gfx.max_draws_3d is 0)", what);
    return Error::make(ErrorCode::Unsupported, "%s: 3D is off (max_draws_3d = 0)", what);
}

} // namespace detail

Result<Mesh> create_mesh(const MeshDesc& desc) {
    const char* what = "create_mesh()";
    if (!detail::mesh_table_ready()) return detail::mesh_3d_off(what);
    const detail::mesh::BuildInfo info = detail::mesh::measure(desc);
    if (!info.ok) {
        NNH_LOG_WARN(gfx, "%s: %s", what, info.error);
        return Error::make(ErrorCode::InvalidArgument, "%s: %s", what, info.error);
    }
    auto* bytes = static_cast<std::uint8_t*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(meshes), info.size, 4));
    if (bytes == nullptr) return Error::make(ErrorCode::OutOfMemory, "%s: no memory to build the mesh", what);
    detail::mesh::MeshView view;
    char why[64];
    Error error;
    detail::MeshRecord* record = nullptr;
    if (detail::mesh::write(desc, {}, bytes, info.size) &&
        detail::mesh::validate(bytes, info.size, view, why, sizeof why)) {
        record = detail::mesh_record_create(view, what, error);
    } else {
        error = Error::make(ErrorCode::InvalidArgument, "%s: the built mesh didn't validate", what);
    }
    mem::free(bytes);
    if (record == nullptr) return error;
    for (int s = 0; s < record->submesh_count; ++s) // the game's materials as given (TextureIds included)
        if (desc.submeshes != nullptr) record->submeshes[s].material = desc.submeshes[s].material;
    return detail::mesh_publish(record, what);
}

} // namespace nnh
