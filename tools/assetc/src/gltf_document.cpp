// glTF container, document and buffers (gltf_model.hpp): .glb chunks, the JSON, version, limits, required
// extensions, notes for what is ignored, and the buffers (the .glb's BIN chunk or `<model>.bin` / `<model>_*.bin`
// files beside the model, plan P25; no data URIs).

#include <cmath>
#include <cstring>

#include "gltf_model.hpp"
#include "paths.hpp"

namespace nnh::assetc::gltf {
namespace {

std::uint32_t u32(const std::string& s, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, s.data() + at, 4);
    return v;
}

bool too_long(json::Ref array, int limit) { return array.is_array() && array.size() > limit; }

void note_ignored(Model& model) {
    const json::Ref root = model.doc.root();
    const char* ignored[][2] = {{"skins", "skins (the model is converted as a static mesh)"},
                                {"animations", "animations"},
                                {"cameras", "cameras"}};
    for (const auto& i : ignored)
        if (root.get(i[0]).size() > 0) model.notes.push_back(std::string("ignored: ") + i[1]);
    for (json::Ref e = root.get("extensionsUsed").first(); e; e = e.next()) {
        const std::string name(e.string());
        if (name != "KHR_materials_unlit") model.notes.push_back("ignored: extension " + name);
    }
}

bool read_buffer(Context& ctx, Model& model, json::Ref buffer, int index, std::size_t& total) {
    const std::string where = " (at $.buffers[" + std::to_string(index) + "])";
    const double length = buffer.get("byteLength").number();
    if (!buffer.get("byteLength").is_number() || length < 1 || length > double(kMaxBufferBytes) ||
        length != std::floor(length))
        return ctx.fail(model.file, "a buffer needs a byteLength from 1 to 32 MiB" + where);
    const auto bytes = static_cast<std::size_t>(length);
    total += bytes;
    if (total > kMaxBufferBytes) return ctx.fail(model.file, "the buffers hold more than 32 MiB" + where);
    const json::Ref uri = buffer.get("uri");
    if (!uri) {
        if (index != 0 || model.glb_bin.size() < bytes)
            return ctx.fail(model.file, "a buffer without a uri must be a .glb's binary chunk" + where);
        model.buffers.push_back(model.glb_bin.substr(0, bytes));
        return true;
    }
    const std::string name(uri.string());
    if (name.rfind("data:", 0) == 0)
        return ctx.fail(model.file, "embedded data (a data: URI) isn't supported: export the .bin beside the model" + where);
    const std::string stem = model.base_name;
    const bool named = name == stem + ".bin" ||
                       (name.size() > stem.size() + 5 && name.compare(0, stem.size() + 1, stem + "_") == 0 &&
                        name.compare(name.size() - 4, 4, ".bin") == 0 && name.find('/') == std::string::npos &&
                        name.find('\\') == std::string::npos);
    if (!named) return ctx.fail(model.file, "buffer '" + name + "' must be named " + stem + ".bin or " + stem + "_<name>.bin beside the model" + where);
    const std::string path = join_path(model.folder, name);
    std::string data;
    if (!ctx.files.read(path, bytes, data) || data.size() < bytes)
        return ctx.fail(model.file, "buffer '" + name + "' is missing or shorter than its byteLength" + where);
    ctx.depend_on(path);
    model.buffers.push_back(std::move(data));
    return true;
}

} // namespace

DMat4 mul(const DMat4& a, const DMat4& b) {
    DMat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            const double s01 = a.m[row] * b.m[c * 4] + a.m[4 + row] * b.m[c * 4 + 1];
            const double s012 = s01 + a.m[8 + row] * b.m[c * 4 + 2];
            r.m[c * 4 + row] = s012 + a.m[12 + row] * b.m[c * 4 + 3];
        }
    return r;
}

bool get_index(json::Ref object, const char* key, int limit, int& out) {
    const json::Ref v = object.get(key);
    if (!v.is_number()) return false;
    const double d = v.number();
    if (!(d >= 0) || d >= limit || d != std::floor(d)) return false;
    out = static_cast<int>(d);
    return true;
}

bool read_container(Context& ctx, const std::string& file, const std::string& bytes, Model& model) {
    std::string json_text = bytes;
    if (bytes.size() >= 4 && bytes.compare(0, 4, "glTF") == 0) {
        if (bytes.size() > kMaxGlbBytes) return ctx.fail(file, "a .glb is limited to 32 MiB");
        if (bytes.size() < 20 || u32(bytes, 4) != 2 || u32(bytes, 8) != bytes.size())
            return ctx.fail(file, "not a glTF 2.0 binary (bad header or length)");
        const std::uint32_t json_len = u32(bytes, 12);
        if (u32(bytes, 16) != 0x4E4F534Au || json_len > bytes.size() - 20)
            return ctx.fail(file, "the first chunk of a .glb must be its JSON");
        json_text = bytes.substr(20, json_len);
        const std::size_t next = 20 + static_cast<std::size_t>(json_len);
        if (next + 8 <= bytes.size()) {
            const std::uint32_t bin_len = u32(bytes, next);
            if (u32(bytes, next + 4) != 0x004E4942u || bin_len > bytes.size() - next - 8)
                return ctx.fail(file, "the second chunk of a .glb must be its binary data");
            model.glb_bin = bytes.substr(next + 8, bin_len);
        }
    } else if (bytes.size() > kMaxGltfBytes) {
        return ctx.fail(file, "a .gltf is limited to 8 MiB");
    }
    if (!model.doc.parse(json_text)) return ctx.fail(file, model.doc.error());
    return true;
}

bool read_document(Context& ctx, Model& model) {
    const json::Ref root = model.doc.root();
    if (!root.is_object()) return ctx.fail(model.file, "the document isn't a JSON object");
    const std::string version(root.get("asset").get("version").string());
    if (version.rfind("2.", 0) != 0) return ctx.fail(model.file, "not glTF 2.0 (asset.version is '" + version + "')");
    for (json::Ref e = root.get("extensionsRequired").first(); e; e = e.next())
        if (e.string() != "KHR_materials_unlit")
            return ctx.fail(model.file, "required extension " + std::string(e.string()) +
                                            " isn't supported (export without compression or texture transforms)");
    const struct {
        const char* key;
        int limit;
    } limits[] = {{"buffers", kMaxBuffers},     {"bufferViews", kMaxViews},   {"accessors", kMaxAccessors},
                  {"meshes", kMaxMeshes},       {"nodes", kMaxNodes},         {"scenes", kMaxScenes},
                  {"materials", kMaxMaterials}, {"textures", kMaxMaterials},  {"images", kMaxMaterials},
                  {"samplers", kMaxMaterials}};
    for (const auto& l : limits)
        if (too_long(root.get(l.key), l.limit))
            return ctx.fail(model.file, std::string("more than ") + std::to_string(l.limit) + " " + l.key);
    note_ignored(model);
    std::size_t total = 0;
    int index = 0;
    for (json::Ref b = root.get("buffers").first(); b; b = b.next(), ++index)
        if (!read_buffer(ctx, model, b, index, total)) return false;
    return true;
}

} // namespace nnh::assetc::gltf
