// A data file's values, one kind at a time (data_encode.hpp): written in the converted format's encoding
// (engine/core/src/data/data_format.hpp) after the checks of FR-5, FR-7 and FR-8.

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "assets/asset_path.hpp"
#include "data/enum_names.hpp"
#include "data_encode.hpp"
#include "nnh/detail/name_hash.hpp"
#include "suggest.hpp"

namespace nnh::assetc {

namespace {

using detail::FieldInfo;
using detail::FieldKind;
using toml::Type;

void wrong(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    e.fail(v.line, path + ": expected " + expected_text(f) + ", found " + found_text(v));
}

std::string number_text(double v, bool integer) {
    char text[40];
    if (integer)
        std::snprintf(text, sizeof text, "%lld", static_cast<long long>(v));
    else
        std::snprintf(text, sizeof text, "%g", v);
    return text;
}

/// "from 1 to 99" / "at least 0" / "at most 1": the bounds narrower than the member type's.
std::string bounds_text(const FieldInfo& f, double lowest, double highest) {
    const bool integer = f.kind == FieldKind::Int;
    const bool low = f.min > lowest;
    const bool high = f.max < highest;
    if (low && high) return "from " + number_text(f.min, integer) + " to " + number_text(f.max, integer);
    return low ? "at least " + number_text(f.min, integer) : "at most " + number_text(f.max, integer);
}

void integer(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    if (v.type != Type::Integer) return wrong(e, f, v, path);
    const int bits = 8 * f.width;
    const double span = static_cast<double>(1ull << bits);
    const double lowest = f.is_signed ? -span / 2 : 0;
    const double highest = f.is_signed ? span / 2 - 1 : span - 1;
    const auto value = static_cast<double>(v.integer);
    if (v.out_of_range || value < lowest || value > highest) {
        char type[8];
        std::snprintf(type, sizeof type, "%c%d", f.is_signed ? 'i' : 'u', bits);
        return e.fail(v.line, path + ": " + v.text + " doesn't fit " + (f.is_signed ? "an " : "a ") + type +
                                  " (" + number_text(lowest, true) + " to " + number_text(highest, true) +
                                  ")");
    }
    if (value < f.min || value > f.max) {
        return e.fail(v.line, path + ": " + v.text + " is out of range: " + bounds_text(f, lowest, highest));
    }
    e.le(static_cast<std::uint32_t>(v.integer), f.width);
}

/// A float from a Float or an Integer value; false (reported) when it can't be one.
bool to_float(Encoder& e, const toml::Value& v, const std::string& path, float& out) {
    if (v.type == Type::Float) {
        if (v.out_of_range) e.fail(v.line, path + ": " + v.text + " is outside a float's range");
        out = v.number;
        return !v.out_of_range;
    }
    out = static_cast<float>(v.integer);
    const double back = static_cast<double>(out);
    if (v.out_of_range || std::fabs(back) >= 9.2e18 || static_cast<std::int64_t>(back) != v.integer) {
        e.fail(v.line, path + ": " + v.text + " can't be stored exactly in a float");
        return false;
    }
    return true;
}

void write_float(Encoder& e, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    e.le(bits, 4);
}

void number(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    if (v.type != Type::Float && v.type != Type::Integer) return wrong(e, f, v, path);
    float value = 0;
    if (!to_float(e, v, path, value)) return;
    if (value < f.min || value > f.max) {
        return e.fail(v.line,
                      path + ": " + v.text + " is out of range: " +
                          bounds_text(f, -static_cast<double>(FLT_MAX), static_cast<double>(FLT_MAX)));
    }
    write_float(e, value);
}

void vec2(Encoder& e, const toml::Value& v, const std::string& path) {
    const auto numeric = [](const toml::Value& x) {
        return x.type == Type::Float || x.type == Type::Integer;
    };
    if (v.type != Type::Array || v.items.size() != 2 || !numeric(v.items[0]) || !numeric(v.items[1])) {
        return e.fail(v.line, path + ": expected [x, y]");
    }
    float x = 0;
    float y = 0;
    if (!to_float(e, v.items[0], path, x) || !to_float(e, v.items[1], path, y)) return;
    write_float(e, x);
    write_float(e, y);
}

int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void color(Encoder& e, const toml::Value& v, const std::string& path) {
    const std::string& t = v.text;
    bool ok = v.type == Type::String && (t.size() == 7 || t.size() == 9) && t[0] == '#';
    for (std::size_t i = 1; ok && i < t.size(); ++i)
        ok = hex(t[i]) >= 0;
    if (!ok) return e.fail(v.line, path + ": expected \"#RRGGBB\" or \"#RRGGBBAA\"");
    for (std::size_t i = 1; i < 9; i += 2)
        e.u8(i < t.size() ? static_cast<std::uint32_t>(hex(t[i]) * 16 + hex(t[i + 1])) : 255);
}

void string(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    if (v.type != Type::String) return wrong(e, f, v, path);
    if (v.text.size() > f.capacity) {
        return e.fail(v.line, path + ": " + std::to_string(v.text.size()) + " bytes, at most " +
                                  std::to_string(f.capacity));
    }
    e.u8(static_cast<std::uint32_t>(v.text.size()));
    e.body.insert(e.body.end(), v.text.begin(), v.text.end());
}

void enumeration(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    if (v.type != Type::String) return wrong(e, f, v, path);
    const detail::EnumInfo* info = detail::find_enum(e.schema.registry(), f.target);
    if (info == nullptr) return e.fail(v.line, path + ": unknown enum");
    const int index = detail::enum_index(info->names, v.text.c_str(), static_cast<int>(v.text.size()));
    if (index < 0) {
        std::vector<std::string> names;
        for (int i = 0; i < info->count; ++i) {
            const detail::NameView n = detail::enum_name(info->names, i);
            names.emplace_back(n.text, static_cast<std::size_t>(n.length));
        }
        const std::string hint = closest(v.text, names);
        return e.fail(v.line, path + ": unknown " + info->name + " '" + v.text + "'" +
                                  (hint.empty() ? "" : " (did you mean '" + hint + "'?)"));
    }
    e.u8(static_cast<std::uint32_t>(index));
}

void reference(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    if (v.type != Type::String) return wrong(e, f, v, path);
    if (v.text.empty()) return e.le(0, 4);
    if (!detail::valid_asset_path(v.text.c_str())) {
        return e.fail(v.line, path + ": '" + v.text + "' isn't an asset path");
    }
    const std::uint32_t hash = detail::fnv1a32(v.text.c_str());
    if (hash == 0) {
        return e.fail(v.line, path + ": the path '" + v.text +
                                  "' has the hash 0, which means none: rename the asset");
    }
    DataReference r;
    r.line = v.line;
    r.kind = f.kind;
    r.field = path;
    r.path = v.text;
    if (f.kind == FieldKind::DataRef) {
        const detail::TypeInfo* target = detail::find_type(e.schema.registry(), f.target);
        r.target = target != nullptr ? target->key : "";
    }
    e.references.push_back(std::move(r));
    e.le(hash, 4);
}

} // namespace

const char* expected_text(const FieldInfo& f) {
    switch (f.kind) {
    case FieldKind::Bool:
        return "true or false";
    case FieldKind::Int:
        return "an integer";
    case FieldKind::Float:
        return "a number";
    case FieldKind::String:
        return "a string";
    case FieldKind::Enum:
        return "a name in quotes";
    case FieldKind::Vec2:
        return "[x, y]";
    case FieldKind::Color:
        return "\"#RRGGBB\" or \"#RRGGBBAA\"";
    case FieldKind::Struct:
        return "a section";
    default:
        return "a path in quotes";
    }
}

const char* found_text(const toml::Value& v) {
    switch (v.type) {
    case Type::Bool:
        return "a boolean";
    case Type::Integer:
        return "an integer";
    case Type::Float:
        return "a float";
    case Type::String:
        return "a string";
    default:
        return "an array";
    }
}

void encode_element(Encoder& e, const FieldInfo& f, const toml::Value& v, const std::string& path) {
    switch (f.kind) {
    case FieldKind::Bool:
        if (v.type != Type::Bool) return wrong(e, f, v, path);
        return e.u8(v.boolean ? 1 : 0);
    case FieldKind::Int:
        return integer(e, f, v, path);
    case FieldKind::Float:
        return number(e, f, v, path);
    case FieldKind::String:
        return string(e, f, v, path);
    case FieldKind::Enum:
        return enumeration(e, f, v, path);
    case FieldKind::Vec2:
        return vec2(e, v, path);
    case FieldKind::Color:
        return color(e, v, path);
    case FieldKind::Struct:
        return e.fail(v.line, path + ": expected a section");
    default:
        return reference(e, f, v, path);
    }
}

} // namespace nnh::assetc
