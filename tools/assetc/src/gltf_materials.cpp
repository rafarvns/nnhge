// glTF materials (gltf_model.hpp): baseColorFactor, baseColorTexture (texture → image → a PNG inside the assets folder,
// power-of-two sides 8..1024), the sampler's filter and wrap, alphaMode/alphaCutoff, doubleSided, KHR_materials_unlit;
// PBR and the other maps are noted and ignored. Images inside the file (bufferView) or data URIs are refused.

#include <cmath>

#include "gltf_model.hpp"
#include "paths.hpp"
#include "png_size.hpp"

namespace nnh::assetc::gltf {
namespace {

json::Ref at(json::Ref array, int index) {
    json::Ref r = array.first();
    for (int i = 0; i < index && r; ++i) r = r.next();
    return r;
}

std::uint8_t unit_to_byte(double v) {
    const double c = v < 0 ? 0 : (v > 1 ? 1 : v);
    return static_cast<std::uint8_t>(std::floor(c * 255.0 + 0.5));
}

nnh::TextureWrap wrap_of(double mode) {
    if (mode == 33071) return nnh::TextureWrap::Clamp;
    if (mode == 33648) return nnh::TextureWrap::Mirror;
    return nnh::TextureWrap::Repeat;
}

bool power_of_two(int v) { return v >= 8 && v <= 1024 && (v & (v - 1)) == 0; }

/// The texture's PNG as an asset path ("models/ship"); its sampler's settings into `m`.
bool read_texture(Context& ctx, Model& model, int index, const std::string& where, nnh::Material3D& m,
                  std::vector<std::string>& textures, int& out) {
    const json::Ref root = model.doc.root();
    const json::Ref tex = at(root.get("textures"), index);
    int image = 0;
    if (!tex.is_object() || !get_index(tex, "source", root.get("images").size(), image))
        return ctx.fail(model.file, "a texture without a valid image (at " + where + ")");
    const json::Ref img = at(root.get("images"), image);
    const std::string img_where = " (at $.images[" + std::to_string(image) + "])";
    if (img.get("bufferView") || !img.get("uri").is_string())
        return ctx.fail(model.file, "images inside the model aren't supported: export the PNG beside it" + img_where);
    const std::string uri(img.get("uri").string());
    if (uri.rfind("data:", 0) == 0) return ctx.fail(model.file, "embedded data (a data: URI) isn't supported" + img_where);
    if (is_absolute_path(uri)) return ctx.fail(model.file, "image '" + uri + "' must be a relative path" + img_where);
    const std::string path = join_path(model.folder, uri);
    std::string inside;
    if (path.size() < 5 || path.compare(path.size() - 4, 4, ".png") != 0)
        return ctx.fail(model.file, "image '" + uri + "' must be a PNG" + img_where);
    if (!path_inside(ctx.assets_root, path, inside))
        return ctx.fail(model.file, "image '" + uri + "' is outside the assets folder" + img_where);
    std::string header;
    int w = 0, h = 0;
    if (!ctx.files.read(path, kPngHeaderBytes, header)) return ctx.fail(model.file, "image '" + uri + "' not found" + img_where);
    if (!png_size(header, w, h)) return ctx.fail(model.file, "image '" + uri + "' is not a PNG file" + img_where);
    if (!power_of_two(w) || !power_of_two(h))
        return ctx.fail(model.file, "image '" + uri + "' is " + std::to_string(w) + "x" + std::to_string(h) +
                                        ": texture sides must be powers of two from 8 to 1024" + img_where);
    ctx.depend_on(path);
    const std::string asset = inside.substr(0, inside.size() - 4);
    out = -1;
    for (std::size_t i = 0; i < textures.size(); ++i)
        if (textures[i] == asset) out = static_cast<int>(i);
    if (out < 0) {
        if (textures.size() >= static_cast<std::size_t>(nnh::kMaxMeshTextures))
            return ctx.fail(model.file, "a model can use at most 8 textures");
        textures.push_back(asset);
        out = static_cast<int>(textures.size()) - 1;
    }
    int sampler = 0;
    m.filter = nnh::TextureFilter::Linear; // no sampler: smooth, as Blender shows it
    if (get_index(tex, "sampler", root.get("samplers").size(), sampler)) {
        const json::Ref s = at(root.get("samplers"), sampler);
        if (s.get("magFilter").number() == 9728) m.filter = nnh::TextureFilter::Nearest;
        m.wrap_u = s.get("wrapS") ? wrap_of(s.get("wrapS").number()) : nnh::TextureWrap::Repeat;
        m.wrap_v = s.get("wrapT") ? wrap_of(s.get("wrapT").number()) : nnh::TextureWrap::Repeat;
    }
    return true;
}

bool read_material(Context& ctx, Model& model, json::Ref mat, int index, MaterialInfo& info,
                   std::vector<std::string>& textures) {
    const std::string where = "$.materials[" + std::to_string(index) + "]";
    nnh::Material3D& m = info.material;
    const json::Ref pbr = mat.get("pbrMetallicRoughness");
    if (const json::Ref f = pbr.get("baseColorFactor")) {
        if (!f.is_array() || f.size() != 4) return ctx.fail(model.file, "baseColorFactor needs 4 numbers (at " + where + ")");
        std::uint8_t c[4];
        int i = 0;
        for (json::Ref v = f.first(); v; v = v.next()) c[i++] = unit_to_byte(v.number());
        m.base_color = {c[0], c[1], c[2], c[3]};
    }
    int tex = 0;
    if (const json::Ref t = pbr.get("baseColorTexture")) {
        if (!get_index(t, "index", model.doc.root().get("textures").size(), tex))
            return ctx.fail(model.file, "baseColorTexture names a texture that doesn't exist (at " + where + ")");
        if (t.get("texCoord").number() > 0) model.notes.push_back("ignored: a second texture coordinate set (" + where + ")");
        if (!read_texture(ctx, model, tex, where, m, textures, info.texture)) return false;
    }
    const std::string mode(mat.get("alphaMode").string());
    if (mode == "MASK") m.alpha = nnh::AlphaMode::Cutout;
    else if (mode == "BLEND") m.alpha = nnh::AlphaMode::Blend;
    else if (!mode.empty() && mode != "OPAQUE") return ctx.fail(model.file, "unknown alphaMode '" + mode + "' (at " + where + ")");
    m.alpha_cutoff = mat.get("alphaCutoff") ? unit_to_byte(mat.get("alphaCutoff").number()) : 128;
    m.cull_back = !mat.get("doubleSided").boolean();
    m.lit = !mat.get("extensions").get("KHR_materials_unlit");
    for (const char* map : {"normalTexture", "occlusionTexture", "emissiveTexture"})
        if (mat.get(map)) model.notes.push_back(std::string("ignored: ") + map + " (" + where + ")");
    if (pbr.get("metallicRoughnessTexture")) model.notes.push_back("ignored: metallicRoughnessTexture (" + where + ")");
    return true;
}

} // namespace

bool read_materials(Context& ctx, Model& model, std::vector<MaterialInfo>& materials, std::vector<std::string>& textures) {
    int index = 0;
    for (json::Ref mat = model.doc.root().get("materials").first(); mat; mat = mat.next(), ++index) {
        MaterialInfo info;
        if (!mat.is_object()) return ctx.fail(model.file, "a material isn't an object");
        if (!read_material(ctx, model, mat, index, info, textures)) return false;
        materials.push_back(info);
    }
    return true;
}

} // namespace nnh::assetc::gltf
