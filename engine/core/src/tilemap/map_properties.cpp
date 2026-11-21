// Properties of a map (spec 012 FR-6): the read-only Properties view and Tilemap's typed setters.

#include <cstring>

#include "tilemap/map_property_table.hpp"
#include "tilemap/map_strings.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;
using detail::MapView;
using detail::PropertyRange;
using Kind = PropertyOwner::Kind;

MapView view_of(const std::uint8_t* block) { return {const_cast<std::uint8_t*>(block)}; }

/// The owner's range. False for an unknown owner. A tile without properties yet gives a null range.
bool owner_range(const MapView& v, PropertyOwner owner, PropertyRange& out) {
    const Header& h = v.header();
    const int i = owner.index;
    switch (owner.kind) {
    case Kind::Map:
        out = {&v.header().properties_first, &v.header().properties_count};
        return true;
    case Kind::TileLayer:
        if (i < 0 || i >= h.tile_layers) return false;
        out = {&v.tile_layers()[i].properties_first, &v.tile_layers()[i].properties_count};
        return true;
    case Kind::ObjectLayer:
        if (i < 0 || i >= h.object_layers) return false;
        out = {&v.object_layers()[i].properties_first, &v.object_layers()[i].properties_count};
        return true;
    case Kind::Object:
        if (i < 0 || i >= h.objects) return false;
        out = {&v.objects()[i].properties_first, &v.objects()[i].properties_count};
        return true;
    case Kind::Tile: {
        if (i < 1 || i > h.tile_ids) return false;
        const int entry = detail::find_tile_props(v, i);
        out = entry < 0 ? PropertyRange{}
                        : PropertyRange{&v.tile_props()[entry].properties_first, &v.tile_props()[entry].properties_count};
        return true;
    }
    }
    return false;
}

Error invalid(const char* what) { return detail::map_refuse(ErrorCode::InvalidArgument, what); }
Error full(const char* what) { return detail::map_refuse(ErrorCode::LimitReached, what); }

/// Adds or replaces `name` on `owner` with `type` and `value` (a string offset is resolved from `text`).
Result<void> set_property(const Tilemap& map, PropertyOwner owner, const char* name, nnh::PropertyType type,
                          std::uint32_t value, const char* text = nullptr) {
    const MapView v = detail::map_view(map);
    if (v.base == nullptr) return invalid("set property on an empty map");
    PropertyRange range;
    if (!owner_range(v, owner, range)) return invalid("no such property owner");
    if (!detail::map_string_ok(name) || name[0] == '\0') return invalid("property name empty or over 255 bytes");
    if (type == nnh::PropertyType::String && !detail::map_string_ok(text)) return invalid("string value over 255 bytes");
    Header& h = v.header();
    int index = range.count ? detail::find_property(v, *range.first, *range.count, name) : -1;
    if (index < 0) {
        if (h.properties >= h.properties_cap) return full("max_properties reached");
        if (range.count == nullptr && h.tile_props >= h.tile_props_cap) return full("too many tiles with properties");
    }
    const int name_at = index < 0 ? detail::map_intern(v, name) : 0;
    const int text_at = type == nnh::PropertyType::String ? detail::map_intern(v, text) : 0;
    if (name_at < 0 || text_at < 0) return full("string_bytes full");
    if (index < 0) {
        if (range.count == nullptr) {
            const int entry = detail::add_tile_props(v, owner.index);
            range = {&v.tile_props()[entry].properties_first, &v.tile_props()[entry].properties_count};
        }
        index = detail::insert_property(v, range);
        v.properties()[index].name = static_cast<std::uint16_t>(name_at);
    }
    PropertyEntry& p = v.properties()[index];
    p.type = static_cast<std::uint8_t>(type);
    p.value = type == nnh::PropertyType::String ? static_cast<std::uint32_t>(text_at) : value;
    return {};
}

} // namespace

// --- Properties view ---

namespace {

const PropertyEntry* find(const std::uint8_t* block, int first, int count, const char* name) {
    if (block == nullptr || name == nullptr || count == 0) return nullptr;
    const MapView v = view_of(block);
    const int i = detail::find_property(v, first, count, name);
    return i < 0 ? nullptr : &v.properties()[i];
}

} // namespace

bool Properties::has(const char* name) const { return find(block_, first_, count_, name) != nullptr; }

nnh::PropertyType Properties::type(const char* name) const {
    const PropertyEntry* p = find(block_, first_, count_, name);
    return p ? static_cast<nnh::PropertyType>(p->type) : nnh::PropertyType::None;
}

bool Properties::get_bool(const char* name, bool fallback) const {
    const PropertyEntry* p = find(block_, first_, count_, name);
    return p && p->type == static_cast<std::uint8_t>(nnh::PropertyType::Bool) ? p->value != 0 : fallback;
}

int Properties::get_int(const char* name, int fallback) const {
    const PropertyEntry* p = find(block_, first_, count_, name);
    return p && p->type == static_cast<std::uint8_t>(nnh::PropertyType::Int) ? static_cast<int>(p->value) : fallback;
}

float Properties::get_float(const char* name, float fallback) const {
    const PropertyEntry* p = find(block_, first_, count_, name);
    if (p == nullptr) return fallback;
    if (p->type == static_cast<std::uint8_t>(nnh::PropertyType::Int)) return static_cast<float>(static_cast<int>(p->value));
    if (p->type != static_cast<std::uint8_t>(nnh::PropertyType::Float)) return fallback;
    float f;
    std::memcpy(&f, &p->value, sizeof f);
    return f;
}

const char* Properties::get_string(const char* name, const char* fallback) const {
    const PropertyEntry* p = find(block_, first_, count_, name);
    if (p == nullptr || p->type != static_cast<std::uint8_t>(nnh::PropertyType::String)) return fallback;
    return view_of(block_).string(static_cast<std::uint16_t>(p->value));
}

const char* Properties::name(int index) const {
    if (block_ == nullptr || index < 0 || index >= count_) return "";
    const MapView v = view_of(block_);
    return v.string(v.properties()[first_ + index].name);
}

// --- Tilemap ---

Properties Tilemap::properties(PropertyOwner owner) const {
    const MapView v = detail::map_view(*this);
    PropertyRange range;
    if (v.base == nullptr || !owner_range(v, owner, range) || range.count == nullptr) return {};
    return TilemapAccess::properties(block_, *range.first, *range.count);
}

Result<void> Tilemap::set_bool(PropertyOwner owner, const char* name, bool value) {
    return set_property(*this, owner, name, nnh::PropertyType::Bool, value ? 1u : 0u);
}

Result<void> Tilemap::set_int(PropertyOwner owner, const char* name, int value) {
    return set_property(*this, owner, name, nnh::PropertyType::Int, static_cast<std::uint32_t>(value));
}

Result<void> Tilemap::set_float(PropertyOwner owner, const char* name, float value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return set_property(*this, owner, name, nnh::PropertyType::Float, bits);
}

Result<void> Tilemap::set_string(PropertyOwner owner, const char* name, const char* value) {
    return set_property(*this, owner, name, nnh::PropertyType::String, 0, value);
}

} // namespace nnh
