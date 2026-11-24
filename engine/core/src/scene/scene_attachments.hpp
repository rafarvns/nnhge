#pragma once
// Scene attachments (decision D30, spec 015 FR-15): the scene module's API for other modules that bind an
// object to a scene (first user: a TweenSet bound to a scene). A node lives inside the attaching object; the
// scene keeps an intrusive list (one pointer per Scene) and knows nothing about what is attached. No
// allocation, no virtual.

#include "nnh/scene.hpp"

#include <cstdint>

namespace nnh::detail {

enum class SceneHook : std::uint8_t {
    BeforeUpdate, ///< right before the top scene's update()
    AfterExit,    ///< right after a scene's on_exit() (pop, replace, end of run(), shutdown())
};

struct SceneAttachment {
    SceneAttachment* next = nullptr;
    void (*notify)(SceneAttachment& self, SceneHook hook) = nullptr; ///< set by the owner
    std::uint32_t epoch = 0;                                         ///< the pass in which it was attached
};

/// The scene module's access to Scene::attachments_ (a friend of Scene).
struct SceneAccess {
    static SceneAttachment*& head(Scene& scene) { return scene.attachments_; }
    static std::uint8_t& pending(Scene& scene) { return scene.pending_; } ///< scene_hook.hpp (spec 023)
};

/// The running notification pass, shared by scene_notify.cpp and scene_attachments.cpp. (Two files so that a
/// game without attachments links only scene_notify(), the cost every game with scenes pays.)
struct ScenePass {
    std::uint32_t number = 0; ///< of the pass running, or of the last one
    bool running = false;
    SceneAttachment* cursor = nullptr; ///< the next node the running pass visits
};
extern ScenePass g_scene_pass;

/// Appends `node` (hooks run in attach order). A node attached during a pass waits for the next pass.
void scene_attach(Scene& scene, SceneAttachment& node);
/// Removes `node`; safe during a pass (the pass skips it if it hasn't been visited yet). No-op if not
/// attached.
void scene_detach(Scene& scene, SceneAttachment& node);
/// Calls every attached node's notify (used by SceneStack). One pass at a time.
void scene_notify(Scene& scene, SceneHook hook);

} // namespace nnh::detail
