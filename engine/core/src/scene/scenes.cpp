// The engine's scene stack and nnh::run() (nnh/scene.hpp), and the transition hook (scene_hook.hpp, D38).

#include "app/dev_pause.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/scene.hpp"
#include "scene/scene_attachments.hpp"
#include "scene/scene_hook.hpp"
#include "scene/scene_stack.hpp"
#include "scene/scenes_internal.hpp"

namespace nnh {
namespace {

detail::SceneStack g_stack;

/// A running transition is cancelled (silently) before the stack is cleared.
void cancel_transition() {
    if (detail::g_scene_transition != nullptr) detail::g_scene_transition->cancel();
}

} // namespace

const detail::SceneTransitionHook* detail::g_scene_transition = nullptr;

void detail::scenes_apply_pending() {
    if constexpr (build::kDevToolsEnabled) g_dev_scenes_changed = g_stack.pending() > 0; // the held frame (spec 030)
    g_stack.apply_pending();
}
void detail::scenes_shutdown() {
    cancel_transition();
    g_stack.clear();
}

void detail::scenes_update_top() { g_stack.update_top(); }
void detail::scenes_draw_all() { g_stack.draw_all(); }

void detail::scenes_request_change(SceneChange change, Scene* scene) {
    switch (change) {
    case SceneChange::Push: g_stack.request_push(*scene); break;
    case SceneChange::Replace: g_stack.request_replace(*scene); break;
    case SceneChange::TransitionPop: g_stack.request_transition_pop(); break;
    }
}

std::uint8_t& detail::scene_pending_count(Scene& scene) { return SceneAccess::pending(scene); }

void scenes::push(Scene& scene) { g_stack.request_push(scene); }
void scenes::pop() { g_stack.request_pop(); }
void scenes::replace(Scene& scene) { g_stack.request_replace(scene); }

void scenes::update_and_draw() {
    if (detail::g_scene_transition != nullptr) [[unlikely]] { // a transition runs this frame (D38)
        detail::g_scene_transition->frame();
        return;
    }
    g_stack.update_and_draw();
}

Scene* scenes::top() { return g_stack.top(); }
int scenes::size() { return g_stack.size(); }

void run(Scene& first) {
    scenes::push(first);
    // A transition that popped the last scene finishes its covered phase before run() returns (spec 023 FR-19).
    while (frame() && (g_stack.size() > 0 || detail::g_scene_transition != nullptr)) scenes::update_and_draw();
    cancel_transition();
    g_stack.clear();
}

} // namespace nnh
