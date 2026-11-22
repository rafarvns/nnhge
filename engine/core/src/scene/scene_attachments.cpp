// Scene attachments (D30): attaching and detaching nodes of a scene's intrusive list. Nodes attached during a
// pass wait for the next one (epoch); detaching during a pass moves the pass's cursor past the node.

#include "scene/scene_attachments.hpp"

namespace nnh::detail {

void scene_attach(Scene& scene, SceneAttachment& node) {
    node.next = nullptr;
    node.epoch = g_scene_pass.running ? g_scene_pass.number : 0;
    SceneAttachment** link = &SceneAccess::head(scene);
    while (*link != nullptr)
        link = &(*link)->next;
    *link = &node;
}

void scene_detach(Scene& scene, SceneAttachment& node) {
    for (SceneAttachment** link = &SceneAccess::head(scene); *link != nullptr; link = &(*link)->next) {
        if (*link != &node) continue;
        *link = node.next;
        if (g_scene_pass.cursor == &node) g_scene_pass.cursor = node.next;
        node.next = nullptr;
        return;
    }
}

} // namespace nnh::detail
