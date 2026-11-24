// glTF accessors and buffer views (gltf_model.hpp): component types, normalised integers, strides, every byte range
// checked inside its view and buffer before any read; sparse accessors refused.

#include <cmath>
#include <cstring>

#include "gltf_model.hpp"

namespace nnh::assetc::gltf {
namespace {

int component_bytes(int type) {
    switch (type) {
    case 5120: case 5121: return 1; // (unsigned) byte
    case 5122: case 5123: return 2; // (unsigned) short
    case 5125: case 5126: return 4; // unsigned int, float
    default: return 0;
    }
}

int components_of(std::string_view type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 0;
}

double read_component(const char* p, int type, bool normalized) {
    switch (type) {
    case 5120: { std::int8_t v; std::memcpy(&v, p, 1); return normalized ? (v < -127 ? -1.0 : v / 127.0) : v; }
    case 5121: { std::uint8_t v; std::memcpy(&v, p, 1); return normalized ? v / 255.0 : v; }
    case 5122: { std::int16_t v; std::memcpy(&v, p, 2); return normalized ? (v < -32767 ? -1.0 : v / 32767.0) : v; }
    case 5123: { std::uint16_t v; std::memcpy(&v, p, 2); return normalized ? v / 65535.0 : v; }
    case 5125: { std::uint32_t v; std::memcpy(&v, p, 4); return v; }
    default: { float v; std::memcpy(&v, p, 4); return v; }
    }
}

bool whole(json::Ref v, double lo, double hi, double& out) {
    if (!v) return out = 0, true;
    if (!v.is_number()) return false;
    out = v.number();
    return out >= lo && out <= hi && out == std::floor(out);
}

} // namespace

bool read_accessor(Context& ctx, const Model& model, int index, const std::string& where, AccessorData& out) {
    const json::Ref root = model.doc.root();
    const std::string at = " (at $.accessors[" + std::to_string(index) + "], used by " + where + ")";

    json::Ref a = root.get("accessors").first();
    for (int i = 0; i < index && a; ++i) a = a.next();
    if (!a.is_object()) return ctx.fail(model.file, "accessor " + std::to_string(index) + " doesn't exist (used by " + where + ")");
    if (a.get("sparse")) return ctx.fail(model.file, "sparse accessors aren't supported" + at);
    const int ctype = static_cast<int>(a.get("componentType").number());
    const int csize = component_bytes(ctype);
    const int comps = components_of(a.get("type").string());
    double count = 0, offset = 0;
    if (csize == 0 || comps == 0 || !whole(a.get("count"), 1, 1 << 22, count) || !whole(a.get("byteOffset"), 0, 1 << 30, offset))
        return ctx.fail(model.file, "an accessor needs a known componentType and type and a count" + at);
    int view_index = 0;
    if (!get_index(a, "bufferView", root.get("bufferViews").size(), view_index))
        return ctx.fail(model.file, "an accessor needs a valid bufferView" + at);
    json::Ref view = root.get("bufferViews").first();
    for (int i = 0; i < view_index; ++i) view = view.next();
    int buffer = 0;
    double view_offset = 0, view_length = 0, stride = 0;
    if (!get_index(view, "buffer", static_cast<int>(model.buffers.size()), buffer) ||
        !whole(view.get("byteOffset"), 0, 1 << 30, view_offset) || !whole(view.get("byteLength"), 1, 1 << 30, view_length) ||
        !whole(view.get("byteStride"), 0, 252, stride))
        return ctx.fail(model.file, "a bufferView needs a buffer, a byteLength and a stride up to 252" + at);
    const double element = double(csize) * comps;
    if (stride == 0) stride = element;
    if (stride < element || std::fmod(stride, csize) != 0 || std::fmod(offset, csize) != 0)
        return ctx.fail(model.file, "misaligned accessor (stride or offset)" + at);
    const double last = offset + stride * (count - 1) + element;
    if (last > view_length || view_offset + view_length > double(model.buffers[static_cast<std::size_t>(buffer)].size()))
        return ctx.fail(model.file, "the accessor reads past its bufferView or buffer" + at);
    const bool normalized = a.get("normalized").boolean();
    const char* base = model.buffers[static_cast<std::size_t>(buffer)].data() + static_cast<std::size_t>(view_offset + offset);
    out.count = static_cast<int>(count);
    out.components = comps;
    out.values.resize(static_cast<std::size_t>(count) * comps);
    for (int i = 0; i < out.count; ++i)
        for (int c = 0; c < comps; ++c) {
            const double v = read_component(base + static_cast<std::size_t>(stride) * i + csize * c, ctype, normalized);
            if (!std::isfinite(v)) return ctx.fail(model.file, "the accessor holds a value that isn't finite" + at);
            out.values[static_cast<std::size_t>(i) * comps + c] = v;
        }
    out.components = comps;
    out.component_type = ctype;
    out.normalized = normalized;
    return true;
}

} // namespace nnh::assetc::gltf
