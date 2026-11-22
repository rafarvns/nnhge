// Scene attachments (D30): the notification pass the scene stack runs before the top scene's update() and
// after a scene's on_exit(). Numbered passes (epoch) and a cursor make attach/detach during a pass safe.

#include "nnh/assert.hpp"
#include "scene/scene_attachments.hpp"

namespace nnh::detail {

ScenePass g_scene_pass;

void scene_notify(Scene& scene, SceneHook hook) {
    SceneAttachment* node = SceneAccess::head(scene);
    if (node == nullptr) return; // the common case: nothing attached
    ScenePass& pass = g_scene_pass;
    NNH_ASSERT(!pass.running, "scene attachments notified from inside a notification");
    pass.running = true;
    if (++pass.number == 0) pass.number = 1; // 0 marks nodes attached outside a pass
    while (node != nullptr) {
        pass.cursor = node->next;
        if (node->epoch != pass.number) node->notify(*node, hook);
        node = pass.cursor;
    }
    pass.cursor = nullptr;
    pass.running = false;
}

} // namespace nnh::detail
