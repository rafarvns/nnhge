#pragma once
// Worlds (spec 020): the cold part of a world's block and the helpers its source files share.

#include "entities/entities_internal.hpp"
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
    std::uint32_t hooked = 0; ///< bit k: kind k has a remove hook
    bool clearing = false;
    World* owner = nullptr; ///< the bound World object (bound worlds don't move)
    Scene* scene = nullptr;
    SceneAttachment node{};
    WorldModuleState modules{}; ///< spec 022: the hierarchy's words (last: no earlier field moves)
};

MemTag& entities_memory_tag();

inline unsigned char* component_at(const WorldHot& w, int kind, int slot) {
    return static_cast<unsigned char*>(w.kinds[kind].data) + static_cast<std::size_t>(slot) * w.kinds[kind].size;
}
inline bool world_busy(const WorldHot& w) { return w.iterating > 0 || w.cold->hook_depth > 0; }

/// Marks free entity `slot` (from find_free) live — fresh during an iteration — and returns its handle. Its record
/// already says "no kind, no tag" (create() and every destroy leave it so). Shared by spawn() and world_reserve()
/// (spec 021): one piece of bookkeeping.
inline Entity take_entity(WorldHot& w, int slot) {
    const bool iterating = w.iterating > 0;
    w.entities.take(slot, iterating);
    if (iterating) w.pending = static_cast<std::uint32_t>(w.pending | kPendingFresh);
    std::uint32_t* r = record_at(w, static_cast<std::uint32_t>(slot));
    r[0] |= kEntityLive;
    return make_handle<World>(static_cast<std::uint32_t>(slot), r[0] & 0xFFFFu);
}
/// Gives entity `entity` (record `r`) kind `kind`'s free `slot` (from find_free): live (fresh during an iteration),
/// owned, in the record's slot field and kinds mask. The slot keeps its T{}. Shared by world_add() and
/// world_reserve() (spec 021).
inline void attach_component(WorldHot& w, int kind, int entity, std::uint32_t* r, int slot) {
    KindHot& k = w.kinds[kind];
    const bool iterating = w.iterating > 0;
    k.slots.take(slot, iterating);
    if (iterating) w.pending = static_cast<std::uint32_t>(w.pending | kPendingFresh);
    k.owner[slot] = static_cast<std::uint16_t>(entity);
    record_slots(w, r)[kind] = static_cast<std::uint16_t>(slot);
    r[1] |= 1u << kind;
}

/// Destroys the component and leaves a T{} (the record's slot already cleared), quarantining its slot. Inline: destroy
/// runs it for every component (plan 020, console bench refinement).
inline void detach_component(WorldHot& w, int kind, int slot) {
    KindHot& k = w.kinds[kind];
    const KindCold& kc = w.cold->kinds[kind];
    k.slots.retire(slot);
    k.owner[slot] = kNoSlot;
    k.dirty = true;
    unsigned char* p = component_at(w, kind, slot);
    if (kc.destroy != nullptr) kc.destroy(p);
    kc.construct(p);
}
/// Destroys `e` (or queues it inside a hook). The caller checked it is live and not dying.
void world_destroy(World& world, WorldHot& w, std::uint32_t slot);
/// After an outermost operation: frees a world destroyed during it. True when it did.
bool world_finish(WorldHot& w);
/// The same, with the common case (not destroyed) inline: no call.
inline void world_finish_if_doomed(WorldHot& w) {
    if ((w.pending & kPendingDoomed) != 0) world_finish(w);
}
/// Destroys every object of every kind and frees the block (no hooks).
void world_teardown(WorldHot* w);
/// clear() without the misuse checks (the destructor, the scene's exit).
void world_clear(World& world, WorldHot& w);
void world_unbind(WorldHot& w);

} // namespace detail
} // namespace nnh
