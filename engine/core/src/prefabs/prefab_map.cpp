// Map objects name prefabs (spec 021 FR-9): every object (or one layer's) in map order — its string property `prefab`
// names the prefab, else its type — spawned at its bounds, with the object in the hook's SpawnInfo. An unknown
// explicit name is a level error: logged in every flavour, asserted in Debug/Development. The only prefab source
// that includes <nnh/tilemap.hpp>: a game that never calls these links no tilemap code (FR-16).

#include "nnh/assert.hpp"
#include "nnh/tilemap.hpp"
#include "prefabs/prefab_internal.hpp"

#include <cstdio>

namespace nnh {
namespace detail {
namespace {

/// Logged in every flavour; the assert repeats it (its line is what a Debug run shows last).
void level_error(const Tilemap& map, const MapObject& o, const char* name) {
    const char* layer = map.object_layer_name(o.object_layer);
    char message[200];
    if (name != nullptr) {
        std::snprintf(message, sizeof message, "object %d \"%s\" (layer %s): no prefab \"%s\"", o.id, o.name, layer,
                      name);
    } else {
        std::snprintf(message, sizeof message, "object %d \"%s\" (layer %s): property \"prefab\" is not a string",
                      o.id, o.name, layer);
    }
    NNH_LOG_ERROR(prefabs, "%s", message);
    NNH_ASSERT(false, "%s", message);
}

/// `world` null: the check call (no spawn, no log, no assert, no stats).
MapSpawnSummary map_objects(const PrefabLibrary& library, PrefabBlock* b, World* world, const Tilemap& map,
                            const MapSpawnOptions& options) {
    MapSpawnSummary s;
    if (b == nullptr) {
        if (world != nullptr) prefab_misuse("PrefabLibrary::spawn_map_objects() on an empty or moved-from library");
        return s;
    }
    TypeCache types;
    const int count = map.object_count();
    for (int i = 0; i < count; ++i) {
        const MapObject o = map.object(i);
        if (options.object_layer >= 0 && o.object_layer != options.object_layer) continue;
        Prefab prefab{};
        const PropertyType explicit_type =
            o.properties.count() > 0 ? o.properties.type("prefab") : PropertyType::None;
        if (explicit_type != PropertyType::None) {
            const char* name = explicit_type == PropertyType::String ? o.properties.get_string("prefab", "") : nullptr;
            if (name != nullptr) prefab = library.find(name);
            if (!prefab) {
                if (s.unknown++ == 0) s.first_unknown = i;
                if (world == nullptr) continue;
                ++b->counts.map_unknown;
                level_error(map, o, name);
                continue;
            }
        } else {
            if (o.type[0] != '\0') prefab = types.find(library, o.type);
            if (!prefab) { // doors, spawn points, empty types: the game's own
                ++s.skipped;
                if (world != nullptr) ++b->counts.map_skipped;
                continue;
            }
        }
        if (world == nullptr) {
            ++s.spawned;
            continue;
        }
        SpawnSource source;
        source.has_position = true;
        source.rect_size = o.shape == ObjectShape::Rect;
        source.position = {o.bounds.x, o.bounds.y};
        source.size = {o.bounds.w, o.bounds.h};
        source.object = &o;
        source.object_index = i;
        if (prefab_spawn(b, *world, prefab, &source, nullptr, 0)) {
            ++s.spawned;
        } else if (!world->clearing()) {
            ++s.misses;
        }
    }
    return s;
}

} // namespace
} // namespace detail

MapSpawnSummary PrefabLibrary::spawn_map_objects(World& world, const Tilemap& map, const MapSpawnOptions& options) {
    return detail::map_objects(*this, block_, &world, map, options);
}

MapSpawnSummary PrefabLibrary::check_map_objects(const Tilemap& map, const MapSpawnOptions& options) const {
    return detail::map_objects(*this, block_, nullptr, map, options);
}

} // namespace nnh
