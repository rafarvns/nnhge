#pragma once
// Worlds (spec 020): the cold part of a world's block and the helpers its source files share.

#include "nnh/entities.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "scene/scene_attachments.hpp"

NNH_LOG_CHANNEL_EXTERN(entities);

namespace nnh {

/// Engine code and tests: a world's block.
struct WorldAccess {
    static detail::WorldHot* hot(const World& w) { return w.hot_; }
    static void set(World& w, detail::WorldHot* hot) { w.hot_ = hot; }
};

namespace detail {

using RemoveHook = SmallFunction<void(World&, Entity, void*)>;

struct KindCold {
    void (*construct)(void*) = nullptr;
    void (*destroy)(void*) = nullptr;
    RemoveHook hook{};
};

/// What only out-of-line code reads.
struct WorldCold {
    KindCold* kinds = nullptr;
    std::uint32_t* dying = nullptr;      ///< entity bitmap: destroy() started (hooks running or queued)
    std::uint16_t* queue_next = nullptr; ///< FIFO links of entities destroyed from hooks
    int queue_head = -1;
    int queue_tail = -1;
    int hook_depth = 0; ///< remove hooks running
    bool clearing = false;
    World* owner = nullptr; ///< the bound World object (bound worlds don't move)
    Scene* scene = nullptr;
    SceneAttachment node{};
};

MemTag& entities_memory_tag();

inline unsigned char* component_at(const WorldHot& w, int kind, int slot) {
    return static_cast<unsigned char*>(w.kinds[kind].data) + static_cast<std::size_t>(slot) * w.kinds[kind].size;
}
inline bool world_busy(const WorldHot& w) { return w.iterating > 0 || w.cold->hook_depth > 0; }

/// Destroys the component and leaves a T{} (the record's slot already cleared), quarantining its slot.
void detach_component(WorldHot& w, int kind, int slot);
/// Destroys `e` (or queues it inside a hook). The caller checked it is live and not dying.
void world_destroy(World& world, WorldHot& w, std::uint32_t slot);
/// After an outermost operation: frees a world destroyed during it. True when it did.
bool world_finish(WorldHot& w);
/// Destroys every object of every kind and frees the block (no hooks).
void world_teardown(WorldHot* w);
/// clear() without the misuse checks (the destructor, the scene's exit).
void world_clear(World& world, WorldHot& w);
void world_unbind(WorldHot& w);

} // namespace detail
} // namespace nnh
