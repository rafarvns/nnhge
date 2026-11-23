// Scene-bound pools (spec 020 FR-18, decision D30): the pool's core carries a scene attachment in its node bytes;
// the scene stack notifies it right before the scene's update() (flush) and right after its on_exit() (clear).
// No new kind of notification: the pool is one more user of D30's attachments, as tween sets are.

#include "nnh/assert.hpp"
#include "pool/pool_internal.hpp"
#include "scene/scene_attachments.hpp"

#include <cstddef>
#include <new>
#include <type_traits>

namespace nnh::detail {
namespace {

static_assert(sizeof(SceneAttachment) <= sizeof(PoolCore::node), "PoolCore::node must hold a SceneAttachment");
static_assert(alignof(SceneAttachment) <= alignof(void*), "PoolCore::node is pointer-aligned");
static_assert(std::is_standard_layout_v<PoolCore>, "the attachment's core is found with offsetof");

SceneAttachment& node_of(PoolCore& core) { return *std::launder(reinterpret_cast<SceneAttachment*>(core.node)); }

PoolCore& core_of(SceneAttachment& node) {
    return *reinterpret_cast<PoolCore*>(reinterpret_cast<unsigned char*>(&node) - offsetof(PoolCore, node));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    PoolCore& core = core_of(node);
    if (core.iterating > 0) return; // cannot happen through the scene stack; stay safe
    if (hook == SceneHook::BeforeUpdate) {
        core.slots.flush();
    } else {
        core.clear_fn(core); // the scene left: the pool starts empty when it comes back
    }
}

} // namespace

void pool_bind(PoolCore& core, Scene& scene) {
    if (core.scene != nullptr || core.iterating > 0) {
        pool_misuse("Pool::bind(): already bound, or inside an iteration (ignored)");
        return;
    }
    auto* node = ::new (static_cast<void*>(core.node)) SceneAttachment{};
    node->notify = &on_scene_hook;
    core.scene = &scene;
    scene_attach(scene, *node);
}

void pool_unbind(PoolCore& core) {
    if (core.scene == nullptr) return;
    scene_detach(*core.scene, node_of(core));
    core.scene = nullptr;
}

} // namespace nnh::detail
