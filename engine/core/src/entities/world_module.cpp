// What modules built over worlds read (spec 022, plan 022 "Entities-internal additions"): the per-world module words,
// whether an entity is being destroyed, whether the world is inside an iteration or a hook.

#include "entities/entities_internal.hpp"
#include "entities/world_internal.hpp"

namespace nnh::detail {

WorldModuleState& world_module_state(World& world) {
    static WorldModuleState scratch; // an empty world's: never kept (callers check the world first)
    WorldHot* w = WorldAccess::hot(world);
    if (w == nullptr) {
        scratch = {};
        return scratch;
    }
    return w->cold->modules;
}

bool world_dying(const World& world, Entity e) {
    const WorldHot* w = WorldAccess::hot(world);
    return w != nullptr && live_record(*w, e) != nullptr && bit_set(w->cold->dying, e.slot());
}

bool world_busy(const World& world) {
    const WorldHot* w = WorldAccess::hot(world);
    return w != nullptr && world_busy(*w);
}

} // namespace nnh::detail
