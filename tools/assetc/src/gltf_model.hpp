#pragma once
// glTF 2.0 → .nnhmesh (spec 026 FR-6/FR-7, D42): the pieces shared by the converter's steps. A model is read in
// order — container (gltf_glb.cpp), document and buffers (gltf_document.cpp), accessors (gltf_accessors.cpp), the
// default scene's nodes (gltf_nodes.cpp), materials and textures (gltf_materials.cpp), primitives merged by material
// (gltf_primitives.cpp) — and written by the engine's own mesh builder (convert_mesh.cpp). Baking is done in double:
// the same bits on every host (IEEE + − × ÷ and sqrt only).

#include <cstdint>
#include <string>
#include <vector>

#include "convert_context.hpp"
#include "json.hpp"
#include "nnh/mesh.hpp"

namespace nnh::assetc::gltf {

// Limits (FR-7).
inline constexpr std::size_t kMaxGltfBytes = 8u << 20;
inline constexpr std::size_t kMaxGlbBytes = 32u << 20;
inline constexpr std::size_t kMaxBufferBytes = 32u << 20; ///< all buffers together
inline constexpr int kMaxBuffers = 16;
inline constexpr int kMaxViews = 1024;
inline constexpr int kMaxAccessors = 1024;
inline constexpr int kMaxMeshes = 256;
inline constexpr int kMaxPrimitives = 256;
inline constexpr int kMaxNodes = 1024;
inline constexpr int kMaxNodeDepth = 32;
inline constexpr int kMaxScenes = 16;
inline constexpr int kMaxMaterials = 32; ///< also textures, images, samplers

/// A column-major 4×4 matrix in double.
struct DMat4 {
    double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
DMat4 mul(const DMat4& a, const DMat4& b);

/// The document and its binary data.
struct Model {
    std::string file;             ///< the .gltf/.glb path (normalized), for messages
    std::string folder;           ///< its folder
    std::string base_name;        ///< file name without extension ("ship")
    json::Document doc;
    std::string glb_bin;          ///< the BIN chunk of a .glb
    std::vector<std::string> buffers; ///< buffer bytes, by index
    std::vector<std::string> notes;   ///< ignored features, one line each
};

/// What the merge produces: the builder's arrays and the converted model's texture paths.
struct Arrays {
    std::vector<nnh::Vec3> positions, normals;
    std::vector<nnh::Vec2> uvs;
    std::vector<nnh::Color> colors;
    std::vector<std::uint16_t> indices;
    std::vector<nnh::SubmeshDesc> submeshes;
    std::vector<std::uint8_t> submesh_textures;
    std::vector<std::string> textures; ///< asset paths without ".png"
    bool has_normals = true, has_uvs = false, has_colors = false;
};

/// A material and its texture (an index into Arrays::textures or −1).
struct MaterialInfo {
    nnh::Material3D material{};
    int texture = -1;
};

/// A primitive placed by a node.
struct Placed {
    json::Ref primitive;
    std::string where; ///< "$.meshes[m].primitives[p]"
    DMat4 world;
};

// The steps; each returns false with ctx.fail(model.file, "reason (at $.path)").
bool read_container(Context& ctx, const std::string& file, const std::string& bytes, Model& model);
bool read_document(Context& ctx, Model& model);
bool collect_primitives(Context& ctx, Model& model, std::vector<Placed>& out);
bool read_materials(Context& ctx, Model& model, std::vector<MaterialInfo>& materials, std::vector<std::string>& textures);
bool merge_primitives(Context& ctx, Model& model, const std::vector<Placed>& placed,
                      const std::vector<MaterialInfo>& materials, Arrays& out);

/// An accessor's values as doubles (count × components; normalised integers mapped to 0..1 or −1..1).
struct AccessorData {
    std::vector<double> values;
    int count = 0;
    int components = 0;
    int component_type = 0; ///< 5120..5126
    bool normalized = false;
};
/// Reads accessor `index` for `what` (an attribute name), checking its type against `allowed_types` ("VEC3") and
/// component types. @return false with ctx's problem set.
bool read_accessor(Context& ctx, const Model& model, int index, const std::string& where, AccessorData& out);

/// A non-negative integer member (false when absent or not a whole number ≥ 0 and < limit).
bool get_index(json::Ref object, const char* key, int limit, int& out);

} // namespace nnh::assetc::gltf
