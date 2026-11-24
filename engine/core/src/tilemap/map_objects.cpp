// Object queries of a map (spec 012 FR-5): object layers, objects by index, name and type.

#include <cstring>

#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;

} // namespace

int Tilemap::object_layer_count() const { return block_ ? detail::map_view(*this).header().object_layers : 0; }

const char* Tilemap::object_layer_name(int index) const {
    if (index < 0 || index >= object_layer_count()) return "";
    const detail::MapView v = detail::map_view(*this);
    return v.string(v.object_layers()[index].name);
}

int Tilemap::object_count() const { return block_ ? detail::map_view(*this).header().objects : 0; }

MapObject Tilemap::object(int index) const {
    MapObject out;
    if (index < 0 || index >= object_count()) return out;
    detail::map_object_at(detail::map_view(*this), index, out);
    return out;
}

int Tilemap::find_object(const char* name) const {
    if (name == nullptr) return -1;
    const detail::MapView v = detail::map_view(*this);
    for (int i = 0; i < object_count(); ++i) {
        if (std::strcmp(v.string(v.objects()[i].name), name) == 0) return i;
    }
    return -1;
}

int Tilemap::find_objects(const char* type, int* indices, int capacity) const {
    if (type == nullptr) return 0;
    const detail::MapView v = detail::map_view(*this);
    int found = 0;
    for (int i = 0; i < object_count(); ++i) {
        if (std::strcmp(v.string(v.objects()[i].type), type) != 0) continue;
        if (indices != nullptr && found < capacity) indices[found] = i;
        ++found;
    }
    return found;
}

} // namespace nnh
