// Map objects name prefabs (spec 021 FR-9): every object (or one layer's) in map order — its string property `prefab`
// names the prefab, else its type — spawned at its bounds, with the object in the hook's SpawnInfo. An unknown
// explicit name is a level error: logged in every flavour, asserted in Debug/Development. The only prefab source
// that includes <nnh/tilemap.hpp>: a game that never calls these links no tilemap code (FR-16).
//
// The walk reads the map's object and property entries in place (tilemap_internal.hpp, plan 021 "Optimisation
// round"): per object no 40-byte MapObject through the stack unless it spawns, no strcmp per property (which names
// are "prefab" is memoised by string offset), no find() per object (NameMemo), and the next entries prefetched.

#include "nnh/assert.hpp"
#include "nnh/tilemap.hpp"
#include "prefabs/prefab_internal.hpp"
#include "tilemap/tilemap_internal.hpp"

#include <cstdio>
#include <cstring>

namespace nnh {
namespace detail {
namespace {

using mapfmt::ObjectEntry;
using mapfmt::PropertyEntry;

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

/// Which property names are "prefab", by string offset (a map's names are few; its pool only grows, so an offset's
/// text never changes). Direct-mapped: a collision only costs a strcmp more.
class PrefabNames {
public:
    PrefabNames() {
        for (std::uint32_t& k : key_) k = kEmpty;
    }
    bool is_prefab(std::uint16_t name, const char* strings) {
        const int slot = (name ^ (name >> 4)) & (kSlots - 1);
        if (key_[slot] != name) {
            key_[slot] = name;
            is_[slot] = std::strcmp(strings + name, "prefab") == 0;
        }
        return is_[slot];
    }

private:
    static constexpr int kSlots = 16;
    static constexpr std::uint32_t kEmpty = 0xFFFFFFFFu;
    std::uint32_t key_[kSlots];
    bool is_[kSlots] = {};
};

inline void prefetch(const void* p) {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_prefetch(p);
#else
    (void)p;
#endif
}

constexpr int kObjectsAhead = 4;    ///< 144 bytes: the ARM11 has no hardware prefetcher (plan 020's lesson)
constexpr int kPropertiesAhead = 8; ///< 96 bytes: an object's properties usually follow the previous object's

/// What an object names (FR-9), as Properties::type/get_string and find() would answer, through the call's memos.
struct ObjectNames {
    const PropertyEntry* props;
    const char* strings;
    NameMemo memo{};
    PrefabNames names{};

    enum class Kind { Spawn, Skip, Unknown };

    /// Spawn: `prefab` is set. Skip: the type names no prefab. Unknown: an explicit `prefab` property names none
    /// (`name` is its text, or null when it is not a string).
    Kind resolve(const PrefabLibrary& library, const ObjectEntry& o, Prefab& prefab, const char*& name) {
        const PropertyEntry* p = nullptr;
        for (int k = o.properties_first, end = k + o.properties_count; k < end && p == nullptr; ++k)
            if (names.is_prefab(props[k].name, strings)) p = props + k; // the first one, as find_property
        const auto type = p != nullptr ? static_cast<PropertyType>(p->type) : PropertyType::None;
        if (type != PropertyType::None) {
            name = type == PropertyType::String ? strings + static_cast<std::uint16_t>(p->value) : nullptr;
            if (name != nullptr) prefab = memo.find(library, name);
            return prefab ? Kind::Spawn : Kind::Unknown;
        }
        if (strings[o.type] != '\0') prefab = memo.find(library, strings + o.type);
        return prefab ? Kind::Spawn : Kind::Skip; // doors, spawn points, empty types: the game's own
    }
};

/// Spawns `prefab` at object `index` (built into `object`, which the hook sees). False on a miss.
bool spawn_object(PrefabBlock* b, World& world, Prefab prefab, const MapView& v, int index, MapObject& object) {
    map_object_at(v, index, object);
    SpawnSource source;
    source.has_position = true;
    source.rect_size = object.shape == ObjectShape::Rect;
    source.position = {object.bounds.x, object.bounds.y};
    source.size = {object.bounds.w, object.bounds.h};
    source.object = &object;
    source.object_index = index;
    return static_cast<bool>(prefab_spawn(b, world, prefab, &source, nullptr, 0));
}

/// `world` null: the check call (no spawn, no log, no assert, no stats).
MapSpawnSummary map_objects(const PrefabLibrary& library, PrefabBlock* b, World* world, const Tilemap& map,
                            const MapSpawnOptions& options) {
    MapSpawnSummary s;
    if (b == nullptr) {
        if (world != nullptr) prefab_misuse("PrefabLibrary::spawn_map_objects() on an empty or moved-from library");
        return s;
    }
    const MapView v = map_view(map);
    if (v.base == nullptr) return s;
    // The sections never move (fixed capacities); a hook that sets a property only shifts entries inside them.
    const int count = v.header().objects, property_count = v.header().properties;
    const ObjectEntry* objects = v.objects();
    ObjectNames names{v.properties(), v.strings()};
    MapObject object;
    for (int i = 0; i < count; ++i) {
        if (i + kObjectsAhead < count) {
            prefetch(objects + i + kObjectsAhead);
            prefetch(reinterpret_cast<const unsigned char*>(objects + i + kObjectsAhead) + 32);
        }
        const ObjectEntry& o = objects[i];
        if (options.object_layer >= 0 && o.object_layer != options.object_layer) continue;
        if (o.properties_first + kPropertiesAhead < property_count)
            prefetch(names.props + o.properties_first + kPropertiesAhead);
        Prefab prefab{};
        const char* name = nullptr;
        const ObjectNames::Kind kind = names.resolve(library, o, prefab, name);
        if (kind == ObjectNames::Kind::Unknown) {
            if (s.unknown++ == 0) s.first_unknown = i;
            if (world == nullptr) continue;
            ++b->counts.map_unknown;
            map_object_at(v, i, object);
            level_error(map, object, name);
        } else if (kind == ObjectNames::Kind::Skip) {
            ++s.skipped;
            if (world != nullptr) ++b->counts.map_skipped;
        } else if (world == nullptr || spawn_object(b, *world, prefab, v, i, object)) {
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
