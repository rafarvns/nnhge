// A Tiled object layer → the map model (see tiled_objects.hpp).

#include "tiled_objects.hpp"

#include "json_fields.hpp"
#include "tiled_properties.hpp"

namespace nnh::assetc {

namespace {

namespace fmt = nnh::detail::mapfmt;

/// The first shape-like member NNHGE can't represent, or null.
const char* unsupported_shape(json::Ref o) {
    if (o.get("ellipse").boolean()) return "ellipse objects aren't supported: use a rectangle";
    if (o.get("polygon")) return "polygon objects aren't supported: use rectangles";
    if (o.get("polyline")) return "polyline objects aren't supported: use rectangles or points";
    if (o.get("text")) return "text objects aren't supported: use a rectangle with a string property";
    if (o.get("gid")) return "tile objects aren't supported: use a rectangle with a property naming the tile";
    if (o.get("template")) return "object templates aren't supported: detach the object from its template";
    if (o.get("rotation").number() != 0) return "rotated objects aren't supported: set the rotation to 0";
    return nullptr;
}

bool read_object(json::Ref o, int layer, const std::string& where_layer, const std::string& file, Context& ctx,
                 Object& out) {
    const std::string name(o.get("name").string());
    const double id = o.get("id").number();
    const std::string id_text = id >= 0 && id <= 0x7FFFFFFF ? std::to_string(static_cast<long>(id)) : "?";
    const std::string where = "object " + id_text +
                              (name.empty() ? "" : " (\"" + name + "\")") + " in " + where_layer;
    Fields f(o, where);
    out.id = static_cast<std::uint32_t>(f.required_integer("id", 1, 0x7FFFFFFF));
    out.name = f.string("name");
    out.type = f.string("type");
    if (out.type.empty()) out.type = f.string("class");
    out.x = static_cast<float>(f.number("x", 0, -1e9, 1e9));
    out.y = static_cast<float>(f.number("y", 0, -1e9, 1e9));
    out.w = static_cast<float>(f.number("width", 0, 0, 1e9));
    out.h = static_cast<float>(f.number("height", 0, 0, 1e9));
    out.shape = f.boolean("point", false) ? fmt::ObjectShape::Point : fmt::ObjectShape::Rect;
    out.object_layer = layer;
    if (const char* shape = unsupported_shape(o)) f.refuse(shape);
    if (out.name.size() > static_cast<std::size_t>(fmt::kMaxStringLength) ||
        out.type.size() > static_cast<std::size_t>(fmt::kMaxStringLength)) {
        f.refuse("the name or type is longer than 255 bytes");
    }
    if (!f.ok()) return ctx.fail(file, f.error());
    if (out.shape == fmt::ObjectShape::Point) out.w = out.h = 0;
    std::string error;
    if (!read_properties(o.get("properties"), where, out.properties, error)) return ctx.fail(file, error);
    return true;
}

} // namespace

bool read_object_layer(json::Ref layer, int index, const std::string& file, Context& ctx, ObjectLayer& out,
                       std::vector<Object>& objects) {
    out.name = std::string(layer.get("name").string());
    const std::string where = "layer \"" + out.name + "\"";
    if (out.name.size() > static_cast<std::size_t>(fmt::kMaxStringLength)) {
        return ctx.fail(file, "a layer name is longer than 255 bytes");
    }
    std::string error;
    if (!read_properties(layer.get("properties"), where, out.properties, error)) return ctx.fail(file, error);
    Fields f(layer, where);
    const json::Ref list = f.array("objects");
    if (!f.ok()) return ctx.fail(file, f.error());
    for (json::Ref o = list.first(); o; o = o.next()) {
        Object object;
        if (!read_object(o, index, where, file, ctx, object)) return false;
        objects.push_back(std::move(object));
    }
    return true;
}

} // namespace nnh::assetc
