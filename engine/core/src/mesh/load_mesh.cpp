// Loading converted models (nnh/mesh.hpp load_mesh, FR-8): the asset `<path>.nnhmesh` read through the assets module,
// validated (never trusted), uploaded, its textures loaded and owned by the mesh, the file's bytes freed.

#include <cstdio>
#include <utility>

#include "log/engine_channels.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/assets.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"

namespace nnh {

detail::MeshRecord* detail::mesh_record_from_file(const AssetData& file, const char* what, Error& error) {
    detail::mesh::MeshView view;
    char why[64];
    if (!detail::mesh::validate(file.data(), file.size(), view, why, sizeof why)) {
        NNH_LOG_WARN(gfx, "%s: %s", what, why);
        error = Error::make(ErrorCode::Corrupted, "%s: %s", what, why);
        return nullptr;
    }
    detail::MeshRecord* record = detail::mesh_record_create(view, what, error);
    if (record == nullptr) return nullptr;
    for (int t = 0; t < record->texture_count; ++t) {
        const char* texture_path = view.textures + t * detail::mesh::kPathBytes;
        auto texture = load_texture(texture_path);
        if (!texture) {
            NNH_LOG_WARN(gfx, "%s: texture \"%s\": %s", what, texture_path, texture.error().message);
            detail::mesh_record_destroy(record);
            error = texture.error();
            return nullptr;
        }
        record->textures[t] = std::move(texture.value());
    }
    for (int s = 0; s < record->submesh_count; ++s) {
        const std::uint8_t t = detail::mesh::submesh(view, s).material.texture;
        if (t != detail::mesh::kNoTexture) record->submeshes[s].material.texture = texture_id(record->textures[t]);
    }
    return record;
}

Result<Mesh> load_mesh(const char* path) {
    if (path == nullptr) path = "";
    char what[96];
    std::snprintf(what, sizeof what, "load_mesh(\"%s\")", path);
    if (!detail::mesh_table_ready()) return detail::mesh_3d_off(what);
    char asset[128];
    if (std::snprintf(asset, sizeof asset, "%s.nnhmesh", path) >= static_cast<int>(sizeof asset))
        return Error::make(ErrorCode::InvalidArgument, "%s: the path is too long", what);
    auto file = assets::load(asset);
    if (!file) {
        NNH_LOG_WARN(gfx, "%s: %s", what, file.error().message);
        return file.error();
    }
    Error error;
    detail::MeshRecord* record = detail::mesh_record_from_file(file.value(), what, error);
    if (record == nullptr) return error;
    auto mesh = detail::mesh_publish(record, what);
    if constexpr (build::kDevToolsEnabled) { // spec 031: the mesh reloads with its asset
        if (mesh) detail::mesh_reload_loaded(MeshAccess::slot(mesh.value()), MeshAccess::generation(mesh.value()), asset);
    }
    return mesh;
}

} // namespace nnh
