// Scene-bound worlds (spec 020 FR-18, decision D30): the cold part carries a scene attachment; the scene stack
// notifies it right before the scene's update() (flush) and right after its on_exit() (clear, hooks run). No new
// kind of notification. The bound World object can't move (that asserts), so the hooks get it by reference.

#include "entities/world_internal.hpp"

#include <cstddef>

namespace nnh {
namespace detail {
namespace {

WorldCold& cold_of(SceneAttachment& node) {
    return *reinterpret_cast<WorldCold*>(reinterpret_cast<unsigned char*>(&node) - offsetof(WorldCold, node));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    World& world = *cold_of(node).owner;
    WorldHot& w = *WorldAccess::hot(world);
    if (world_busy(w) || w.cold->clearing) return; // cannot happen through the scene stack; stay safe
    if (hook == SceneHook::BeforeUpdate) {
        world.flush();
    } else {
        world.clear(); // the scene left: nothing it spawned may call back into it
    }
}

} // namespace

void world_unbind(WorldHot& w) {
    WorldCold& c = *w.cold;
    if (c.scene == nullptr) return;
    scene_detach(*c.scene, c.node);
    c.scene = nullptr;
    c.owner = nullptr;
}

} // namespace detail

void World::bind(Scene& scene) {
    if (hot_ == nullptr) {
        detail::world_misuse("use of an empty or moved-from nnh::World");
        return;
    }
    detail::WorldCold& c = *hot_->cold;
    if (c.scene != nullptr || detail::world_busy(*hot_) || c.clearing) {
        detail::world_misuse("World::bind(): already bound, or inside an iteration, a hook or clear() (ignored)");
        return;
    }
    c.node = detail::SceneAttachment{};
    c.node.notify = &detail::on_scene_hook;
    c.scene = &scene;
    c.owner = this;
    detail::scene_attach(scene, c.node);
}

bool World::bound() const { return hot_ != nullptr && hot_->cold->scene != nullptr; }

} // namespace nnh
