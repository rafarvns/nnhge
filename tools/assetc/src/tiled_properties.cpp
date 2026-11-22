// Tiled custom properties → the map model (see tiled_properties.hpp).

#include "tiled_properties.hpp"

#include <cfloat>

#include "json_fields.hpp"

namespace nnh::assetc {

namespace {

using nnh::detail::mapfmt::PropertyType;
using nnh::detail::mapfmt::kMaxStringLength;

/// The value of one property, by its Tiled type.
bool read_value(json::Ref p, Fields& f, const std::string& type, Property& out) {
    if (type == "bool") {
        out.type = PropertyType::Bool;
        if (!p.get("value").is_bool()) return f.refuse("\"value\" must be true or false");
        out.boolean = p.get("value").boolean();
    } else if (type == "int" || type == "object") {
        out.type = PropertyType::Int;
        out.integer = static_cast<std::int32_t>(f.integer("value", 0, INT32_MIN, INT32_MAX));
        if (!p.get("value").is_number()) return f.refuse("\"value\" must be a whole number");
    } else if (type == "float") {
        out.type = PropertyType::Float;
        out.real = static_cast<float>(f.number("value", 0, -FLT_MAX, FLT_MAX));
        if (!p.get("value").is_number()) return f.refuse("\"value\" must be a number");
    } else if (type == "string" || type == "color" || type == "file") {
        out.type = PropertyType::String;
        out.text = f.string("value");
        if (!p.get("value").is_string()) return f.refuse("\"value\" must be a string");
        if (out.text.size() > static_cast<std::size_t>(kMaxStringLength)) {
            return f.refuse("the value is longer than 255 bytes");
        }
    } else if (type == "class") {
        return f.refuse("class properties aren't supported: use plain properties (bool, int, float, string)");
    } else {
        return f.refuse("unknown property type \"" + type + "\"");
    }
    return f.ok();
}

} // namespace

bool read_properties(json::Ref array, const std::string& where, Properties& out, std::string& error) {
    out.clear();
    if (!array) return true;
    if (!array.is_array()) {
        error = where + ": \"properties\" must be an array";
        return false;
    }
    for (json::Ref p = array.first(); p; p = p.next()) {
        Property prop;
        prop.name = std::string(p.get("name").string());
        Fields f(p, where + ": property \"" + prop.name + "\"");
        if (!p.is_object() || prop.name.empty() || prop.name.size() > static_cast<std::size_t>(kMaxStringLength)) {
            error = where + ": a property needs a name of 1 to 255 bytes";
            return false;
        }
        const std::string type = f.string("type", "string");
        if (!f.ok() || !read_value(p, f, type, prop)) {
            error = f.error();
            return false;
        }
        for (const Property& earlier : out) {
            if (earlier.name == prop.name) {
                error = where + ": property \"" + prop.name + "\" is there twice";
                return false;
            }
        }
        out.push_back(std::move(prop));
    }
    return true;
}

bool take_bool(Properties& props, const char* name, const std::string& where, bool& value, std::string& error) {
    value = false;
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (it->name != name) continue;
        if (it->type != PropertyType::Bool) {
            error = where + ": property \"" + name + "\" must be a bool";
            return false;
        }
        value = it->boolean;
        props.erase(it);
        return true;
    }
    return true;
}

} // namespace nnh::assetc
