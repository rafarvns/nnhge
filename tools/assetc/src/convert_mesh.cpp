// One whole model conversion (convert_mesh.hpp): glTF 2.0 → .nnhmesh through the engine's own mesh builder (the same
// bytes as create_mesh from the same arrays, AC-4), checked by the engine's validator.

#include "convert_mesh.hpp"

#include "gltf_model.hpp"
#include "mesh/mesh_build.hpp"
#include "mesh/mesh_validate.hpp"
#include "paths.hpp"

namespace nnh::assetc {

namespace {

bool ends_with(const std::string& s, const char* suffix) {
    const std::string x(suffix);
    return s.size() > x.size() && s.compare(s.size() - x.size(), x.size(), x) == 0;
}

bool build_bytes(Context& ctx, const std::string& file, const gltf::Arrays& a, Converted& out) {
    std::vector<const char*> paths;
    for (const std::string& t : a.textures) paths.push_back(t.c_str());
    const nnh::MeshDesc desc{.positions = a.positions.data(),
                             .vertex_count = static_cast<int>(a.positions.size()),
                             .normals = a.has_normals ? a.normals.data() : nullptr,
                             .uvs = a.has_uvs ? a.uvs.data() : nullptr,
                             .colors = a.has_colors ? a.colors.data() : nullptr,
                             .indices = a.indices.data(),
                             .index_count = static_cast<int>(a.indices.size()),
                             .submeshes = a.submeshes.data(),
                             .submesh_count = static_cast<int>(a.submeshes.size())};
    const detail::mesh::BuildExtras extras{paths.data(), static_cast<int>(paths.size()), a.submesh_textures.data()};
    const detail::mesh::BuildInfo info = detail::mesh::measure(desc, extras);
    if (!info.ok) return ctx.fail(file, info.error);
    out.bytes.resize(info.size);
    detail::mesh::MeshView view;
    char why[64];
    if (!detail::mesh::write(desc, extras, out.bytes.data(), out.bytes.size()) ||
        !detail::mesh::validate(out.bytes.data(), out.bytes.size(), view, why, sizeof why))
        return ctx.fail(file, "internal error: the mesh didn't validate");
    return true;
}

} // namespace

bool convert_mesh(FileSource& files, const std::string& model_path, const std::string& assets_root, Converted& out,
                  Problem& problem, std::vector<std::string>* notes) {
    out = {};
    Context ctx{files, normalize_path(assets_root), {}, {}};
    const std::string file = normalize_path(model_path);
    const bool glb = ends_with(file, ".glb");
    if (!glb && !ends_with(file, ".gltf")) return problem = {file, "a model must be a .gltf or .glb file", 0}, false;
    std::string inside;
    if (!path_inside(ctx.assets_root, file, inside)) {
        problem = {file, "the model is outside the assets folder " + ctx.assets_root, 0};
        return false;
    }
    gltf::Model model;
    model.file = file;
    model.folder = parent_path(file);
    const std::size_t slash = file.rfind('/');
    const std::string name = slash == std::string::npos ? file : file.substr(slash + 1);
    model.base_name = name.substr(0, name.rfind('.'));
    std::string bytes;
    std::vector<gltf::Placed> placed;
    std::vector<gltf::MaterialInfo> materials;
    gltf::Arrays arrays;
    const bool ok = files.read(file, (glb ? gltf::kMaxGlbBytes : gltf::kMaxGltfBytes) + 1, bytes)
                        ? gltf::read_container(ctx, file, bytes, model) && gltf::read_document(ctx, model) &&
                              gltf::collect_primitives(ctx, model, placed) &&
                              gltf::read_materials(ctx, model, materials, arrays.textures) &&
                              gltf::merge_primitives(ctx, model, placed, materials, arrays) &&
                              build_bytes(ctx, file, arrays, out)
                        : ctx.fail(file, "file not found");
    if (!ok) {
        problem = ctx.problem;
        out = {};
        return false;
    }
    out.dependencies.push_back(file);
    for (const std::string& d : ctx.dependencies) out.dependencies.push_back(d);
    if (notes != nullptr) *notes = model.notes;
    return true;
}

} // namespace nnh::assetc
