// Loading collision layer sets (spec 019 FR-6, FR-7): the `collision_layers` int property of a converted map's tile
// layers and tiles becomes their runtime set. The converter refuses a non-int or 0 value; a file holding one anyway
// (an old map with a property of that name, or a damaged one) keeps the default set, with one warning per map.

#include "log/engine_channels.hpp"
#include "nnh/log.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

constexpr const char* kProperty = "collision_layers";

/// The set a property view gives, or 0 when it has none or a bad one (`bad` set then).
collision::Layers read_set(const Properties& p, bool& bad) {
    if (!p.has(kProperty)) return collision::kNoLayers;
    const int value = p.type(kProperty) == PropertyType::Int ? p.get_int(kProperty) : 0;
    if (value == 0) bad = true;
    return static_cast<collision::Layers>(value);
}

} // namespace

void detail::load_collision_layers(Tilemap& map, const char* name) {
    bool bad = false;
    for (int layer = 0; layer < map.tile_layer_count(); ++layer) {
        const collision::Layers set = read_set(map.properties(PropertyOwner::tile_layer(layer)), bad);
        if (set != collision::kNoLayers) map.set_layer_collision_layers(layer, set);
    }
    const detail::MapView v = map_view(map);
    for (int i = 0; i < v.header().tile_props; ++i) {
        const int id = v.tile_props()[i].tile_id;
        const collision::Layers set = read_set(map.properties(PropertyOwner::tile(id)), bad);
        if (set != collision::kNoLayers) map.set_tile_collision_layers(id, set);
    }
    if (bad) {
        NNH_LOG_WARN(tilemap, "load_tilemap(\"%s\"): a collision_layers property isn't a non-zero int: ignored", name);
    }
}

} // namespace nnh
